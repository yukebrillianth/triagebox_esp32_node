/*
 * Real board outputs: backlight + buzzer behind the TCA9554 expander, and
 * power-off via the SW6106 PMIC. The sim/Editor build links
 * ui/logic/ui_board_stub.c instead.
 */
#include "ui_board.h"

#include "bsp/esp32_s3_touch_lcd_4.h"
#include "driver/i2c_master.h"
#include "esp_io_expander.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * EXIO map read from the official V3.0 schematic (net names, not the older
 * prose table which is off by one):
 *   EXIO1=TP_RST  EXIO2=BL_EN  EXIO3=LCD_RST  EXIO4=SDCS
 *   EXIO5=BLC     EXIO6=BEE_EN EXIO7=RTC_INT  EXIO8=IRQ/LED3
 * Schematic labels are 1-based, esp_io_expander pins are 0-based, so
 * EXIO(n) == IO_EXPANDER_PIN_NUM_(n-1). Touch TP_RST or LCD_RST by mistake and
 * the panel resets instead of the light changing.
 */
#define PIN_BL_EN  IO_EXPANDER_PIN_NUM_1 /* EXIO2 */
#define PIN_BEE_EN IO_EXPANDER_PIN_NUM_5 /* EXIO6 */

static const char *TAG = "ui_board";
static esp_io_expander_handle_t s_io;

/*
 * Polarity verified against hardware 2026-08-14 by reading the expander back:
 * config reg 0x03 = 0xdd (only bits 1 and 5 are outputs, i.e. exactly EXIO2 and
 * EXIO6) and output reg 0x01 = 0xdf (bit1 high with the backlight lit, bit5 low
 * with the buzzer silent). Both lines are active-high. If a future board
 * revision inverts one, flip these two and nothing else.
 */
#define BL_ON_LEVEL  1
#define BEE_ON_LEVEL 1

/*
 * SW6106 PMIC on the shared I2C bus. It owns the battery and the 5V/3V3 rails
 * -- board V3.0 has no SYS_EN, and the PMIC's KEY pin goes only to the tactile
 * switch, so this register write is the only way firmware can cut its own power.
 *
 * Register numbers and the write-unlock handshake are from the SW6106 I2C
 * Register List (RG006_1_v1.2), not guessed. Address 0x3c confirmed by bus scan.
 *
 * Verified on hardware: this powers the board down **even with USB connected**.
 * It is a real power cut, not a request -- there is no dry-run mode. Anything
 * that must survive a shutdown has to be persisted before calling this.
 */
#define SW6106_ADDR         0x3cU
#define SW6106_REG_BG_CTRL  0x01U /* [7:6] write-unlock: 1 then 2 */
#define SW6106_REG_KEY_EVT  0x03U /* [4] output power off, self-clearing */
#define SW6106_REG_GAUGE    0x49U /* [3] gates REG 0x03[4]; must read 1 */

#define SW6106_UNLOCK_1     0x40U /* [7:6] = 1, "first operation"  */
#define SW6106_UNLOCK_2     0x80U /* [7:6] = 2, "second operation" */
#define SW6106_POWER_OFF    0x10U /* [4] = 1 */
#define SW6106_GAUGE_KEY_OFF_EN 0x08U /* [3] */

void ui_board_init(void)
{
    /* bsp_io_expander_init() exists in the BSP but nothing ever called it,
     * which is why bsp_display_brightness_set() has always been a no-op. */
    s_io = bsp_io_expander_init();
    if (s_io == NULL) {
        ESP_LOGE(TAG, "TCA9554 init failed; backlight/buzzer stay untouched");
        return;
    }

    /* Only claim the two pins we drive. The rest stay input/high-Z so we cannot
     * fight the panel reset lines or the RTC interrupt. */
    ESP_ERROR_CHECK(esp_io_expander_set_dir(s_io, PIN_BL_EN | PIN_BEE_EN,
                                            IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(s_io, PIN_BEE_EN, !BEE_ON_LEVEL));
    ESP_ERROR_CHECK(esp_io_expander_set_level(s_io, PIN_BL_EN, BL_ON_LEVEL));
    ESP_LOGI(TAG, "expander up: BL_EN=EXIO2 BEE_EN=EXIO6");
}

void ui_board_backlight(bool on)
{
    if (s_io == NULL) {
        return;
    }
    esp_io_expander_set_level(s_io, PIN_BL_EN, on ? BL_ON_LEVEL : !BL_ON_LEVEL);
}

void ui_board_buzzer(bool on)
{
    if (s_io == NULL) {
        return;
    }
    /* Active buzzer: it self-oscillates. It has to be — BEE_EN is behind an I2C
     * expander, so there is no way to toggle it at an audio frequency. */
    esp_io_expander_set_level(s_io, PIN_BEE_EN, on ? BEE_ON_LEVEL : !BEE_ON_LEVEL);
}

/* One register write to the PMIC. Kept tiny so the power-off sequence below
 * reads as the three steps the register list describes. */
static esp_err_t sw6106_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    const uint8_t buf[2] = {reg, val};

    return i2c_master_transmit(dev, buf, sizeof(buf), 100);
}

void ui_board_power_off(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    i2c_master_dev_handle_t dev = NULL;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SW6106_ADDR,
        .scl_speed_hz = 100000,
    };
    uint8_t reg = SW6106_REG_GAUGE;
    uint8_t gauge = 0;
    esp_err_t err;

    if (bus == NULL) {
        ESP_LOGE(TAG, "no I2C bus; cannot reach the PMIC");
        return;
    }
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "could not address the PMIC at 0x%02x", SW6106_ADDR);
        return;
    }

    /*
     * REG 0x03[4] only works when REG 0x49[3] is set. That bit is OTP and reads
     * 1 on this board, but check rather than write a no-op and leave the UI
     * looking like it hung: if the gate is closed we still want the log line.
     */
    err = i2c_master_transmit_receive(dev, &reg, 1, &gauge, 1, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PMIC gauge read failed: %s", esp_err_to_name(err));
        goto out;
    }
    if ((gauge & SW6106_GAUGE_KEY_OFF_EN) == 0U) {
        ESP_LOGE(TAG, "PMIC key-off disabled (0x49=0x%02x); rail stays up", gauge);
        goto out;
    }

    /* Write-unlock, then the event. Nothing is retried: a partial unlock leaves
     * the protection engaged, which is the safe end state. */
    err = sw6106_write(dev, SW6106_REG_BG_CTRL, SW6106_UNLOCK_1);
    if (err == ESP_OK) {
        err = sw6106_write(dev, SW6106_REG_BG_CTRL, SW6106_UNLOCK_2);
    }
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "cutting power now");
        err = sw6106_write(dev, SW6106_REG_KEY_EVT, SW6106_POWER_OFF);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PMIC power-off write failed: %s", esp_err_to_name(err));
        goto out;
    }

    /* The rails go before this returns -- verified on hardware 2026-08-14, and
     * it cuts even with USB attached, so there is no "safe" way to test it. If
     * we are still here, say so: silence would look like a dead button. */
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGE(TAG, "still running 500ms after power-off; PMIC ignored it");

out:
    (void)i2c_master_bus_rm_device(dev);
}
