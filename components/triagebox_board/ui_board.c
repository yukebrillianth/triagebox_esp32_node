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

/*
 * Fuel gauge, read-only. 0x4F[6:0] is the SW6106's own final SoC estimate in 1%
 * steps -- preferred over deriving a percentage from 0x14/0x15 Vbat, because a
 * voltage-to-percent curve for a pack nobody has characterised would just be a
 * guess with more decimal places.
 *
 * No unlock is needed to read: 0x01[7:6] and 0x22[7:6] gate only writes to their
 * own register. Nothing below writes.
 */
#define SW6106_REG_STATUS   0x11U /* [4] charging, [5] discharging */
#define SW6106_REG_VBAT_L   0x14U /* Vbat [7:0]  */
#define SW6106_REG_VBAT_H   0x15U /* Vbat [11:8] in [3:0]; [7:4] belong to Vout */
#define SW6106_REG_SOC      0x4FU /* [6:0] final SoC, 1%/step */
#define SW6106_REG_IDIS_L   0x19U /* Idischg [7:0] */
#define SW6106_REG_IDIS_H   0x18U /* Idischg [11:8] in [7:4]; [3:0] are Ichg's */
#define SW6106_STAT_CHARGING 0x10U /* [4] */
#define SW6106_SOC_MASK     0x7FU
#define SW6106_VBAT_H_MASK  0x0FU

/*
 * Every function here shares SDA GPIO15 / SCL GPIO7 with the GT911, the RTC and
 * the STM32 polled at 20 Hz, so each public entry point takes bsp_i2c_lock()
 * around its whole register sequence -- not per transfer. A single
 * transmit_receive is already indivisible inside the IDF driver; what is not is
 * the SW6106's three-write unlock-then-shutdown, and holding the lock across it
 * is free.
 *
 * 100 ms: longer than any 100 kHz transfer plus one competing sequence, short
 * enough that a wedged bus makes the caller skip a turn instead of hanging.
 * Never portMAX_DELAY -- ui_board_backlight() and ui_board_buzzer() run on the
 * LVGL task, and that is exactly the freeze this lock exists to prevent.
 */
#define UI_BOARD_I2C_TIMEOUT 100

/*
 * One handle, created on first use and never removed: the status bar reads the
 * gauge periodically, and add/remove per read would churn the bus driver's
 * device list for no benefit.
 *
 * ui_board_power_off() deliberately keeps its own short-lived handle instead of
 * sharing this one -- it runs once and ends the world, and its selftest drives
 * the "no bus" and "cannot address" paths repeatedly, which a cached handle would
 * short-circuit.
 */
static i2c_master_dev_handle_t pmic_dev(void)
{
    static i2c_master_dev_handle_t dev;
    i2c_master_bus_handle_t bus;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SW6106_ADDR,
        .scl_speed_hz = 100000,
    };

    if (dev != NULL) {
        return dev;
    }
    bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "no I2C bus; cannot reach the PMIC");
        return NULL;
    }
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "could not address the PMIC at 0x%02x", SW6106_ADDR);
        dev = NULL;
    }
    return dev;
}

/* One addressed read per register: the SW6106's register-pointer auto-increment
 * is undocumented, so a two-byte burst would be a guess about silicon. */
static esp_err_t sw6106_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(dev, &reg, 1, val, 1, 100);
}

bool ui_board_battery(uint8_t *percent, bool *charging)
{
    i2c_master_dev_handle_t dev = pmic_dev();
    uint8_t soc = 0;
    uint8_t status = 0;
    esp_err_t err;

    if (dev == NULL) {
        return false;
    }
    if (!bsp_i2c_lock(UI_BOARD_I2C_TIMEOUT)) {
        /* Same outcome as an unreadable gauge, deliberately: the caller renders
         * "--%" rather than a stale percentage. See the 0xFF rule in
         * tb_link_i2c.c's push_battery_if_due(). */
        ESP_LOGD(TAG, "battery read skipped: I2C bus busy");
        return false;
    }
    err = sw6106_read(dev, SW6106_REG_SOC, &soc);
    if (err == ESP_OK) {
        /* Charge state is a nice-to-have: losing it must not also lose a
         * percentage we did read. */
        if (sw6106_read(dev, SW6106_REG_STATUS, &status) != ESP_OK) {
            status = 0;
        }
    }
    bsp_i2c_unlock();

    if (err != ESP_OK) {
        return false;
    }
    /*
     * 0x4F is spec'd 0-100, so 101-127 is a bad read and must come back as NO
     * reading, not a clamp. The old clamp read a floating bus (0xff) as 100% with
     * a full icon -- the one direction the gauge must never lie in -- and could
     * never return UI_BATTERY_UNKNOWN, because the mask here throws away bit 7
     * before the caller gets to see it.
     */
    soc &= SW6106_SOC_MASK;
    if (soc > 100U) {
        ESP_LOGD(TAG, "gauge SoC 0x%02x out of range; reporting no reading", soc);
        return false;
    }
    if (percent != NULL) {
        *percent = soc;
    }
    if (charging != NULL) {
        *charging = (status & SW6106_STAT_CHARGING) != 0U;
    }
    return true;
}

bool ui_board_battery_mv(uint16_t *millivolts)
{
    i2c_master_dev_handle_t dev = pmic_dev();
    uint8_t vl = 0;
    uint8_t vh = 0;
    uint32_t raw;
    bool ok;

    if (dev == NULL) {
        return false;
    }
    /*
     * Under the lock, like every other entry point here. It was not, and that was
     * a real hazard rather than a tidiness point: two SEPARATE addressed reads
     * make a 12-bit number out of two registers, and the GT911 or the 20 Hz STM32
     * poll landing between them yields a low byte from one sample and a high
     * nibble from another -- a voltage that never existed, off by up to 300 mV
     * across a 256-count boundary. Held across the pair, not per read.
     */
    if (!bsp_i2c_lock(UI_BOARD_I2C_TIMEOUT)) {
        ESP_LOGD(TAG, "pack voltage read skipped: I2C bus busy");
        return false;
    }
    ok = (sw6106_read(dev, SW6106_REG_VBAT_L, &vl) == ESP_OK) &&
         (sw6106_read(dev, SW6106_REG_VBAT_H, &vh) == ESP_OK);
    bsp_i2c_unlock();

    if (!ok) {
        return false;
    }
    raw = ((uint32_t)(vh & SW6106_VBAT_H_MASK) << 8) | (uint32_t)vl;
    /* * 1.2 mV -> * 12 / 10; integer math without float. */
    if (millivolts != NULL) {
        *millivolts = (uint16_t)((raw * 12U) / 10U);
    }
    return true;
}

bool ui_board_battery_ma(uint16_t *milliamps)
{
    i2c_master_dev_handle_t dev = pmic_dev();
    uint8_t il = 0;
    uint8_t ih = 0;
    uint32_t raw;
    bool ok;

    if (dev == NULL) {
        return false;
    }
    /* One lock across both reads, for the reason spelled out above: the two
     * registers are halves of one 12-bit number. */
    if (!bsp_i2c_lock(UI_BOARD_I2C_TIMEOUT)) {
        ESP_LOGD(TAG, "discharge current read skipped: I2C bus busy");
        return false;
    }
    ok = (sw6106_read(dev, SW6106_REG_IDIS_L, &il) == ESP_OK) &&
         (sw6106_read(dev, SW6106_REG_IDIS_H, &ih) == ESP_OK);
    bsp_i2c_unlock();

    if (!ok) {
        return false;
    }
    /* Idischg [11:8] live in the HIGH nibble of 0x18; the low nibble is Ichg's,
     * which is why this shifts rather than masks. */
    raw = ((uint32_t)(ih >> 4) << 8) | (uint32_t)il;
    /* * 25/7 mA per LSB, in that order: 4095*25 = 102375 fits an int32 with room
     * to spare, so this is exact to the nearest mA with no float. Same decode the
     * `pmic` console command prints. */
    if (milliamps != NULL) {
        *milliamps = (uint16_t)((raw * 25U) / 7U);
    }
    return true;
}

void ui_board_init(void)
{
    /*
     * bsp_i2c_get_handle() purely to force bsp_i2c_init(): the lock is created
     * there, and esp_io_expander_new_i2c_tca9554() below does its own I2C
     * transactions that have to be inside it. app_main calls this AFTER
     * bsp_display_start(), so the LVGL task is already polling touch on this bus
     * -- an unlocked collision here leaves the backlight off, which reads as a
     * dead panel (AGENTS.md hardware trap 2).
     */
    if (bsp_i2c_get_handle() == NULL || !bsp_i2c_lock(UI_BOARD_I2C_TIMEOUT)) {
        ESP_LOGE(TAG, "no I2C bus, or bus busy; backlight/buzzer stay untouched");
        return;
    }

    /* bsp_io_expander_init() exists in the BSP but nothing ever called it,
     * which is why bsp_display_brightness_set() has always been a no-op. */
    s_io = bsp_io_expander_init();
    if (s_io == NULL) {
        bsp_i2c_unlock();
        ESP_LOGE(TAG, "TCA9554 init failed; backlight/buzzer stay untouched");
        return;
    }

    /*
     * Only claim the two pins we drive. The rest stay input/high-Z so we cannot
     * fight the panel reset lines or the RTC interrupt.
     *
     * These are the first I2C writes of boot and the one place a wedged bus
     * (the measured STM32-holds-SCL state) can still abort: on this bus three
     * more devices answer nothing, so each write NAKs, and aborting here is a
     * 0 s reboot loop with the backlight still lit -- the recorded "blackscreen,
     * backlight menyala" symptom, because a panic means ui_board_init() never
     * runs again either. So every failure logs loudly and degrades to "no
     * backlight/buzzer control", the same shape as the error paths above, and
     * leaves the pins where the failed write left them: a level that cannot be
     * confirmed is not blindly re-driven.
     */
    if (esp_io_expander_set_dir(s_io, PIN_BL_EN | PIN_BEE_EN,
                                IO_EXPANDER_OUTPUT) != ESP_OK ||
        esp_io_expander_set_level(s_io, PIN_BEE_EN, !BEE_ON_LEVEL) != ESP_OK ||
        esp_io_expander_set_level(s_io, PIN_BL_EN, BL_ON_LEVEL) != ESP_OK) {
        ESP_LOGE(TAG, "expander setup failed: backlight/buzzer now uncontrollable "
                      "-- was the bus wedged? (i2cstate)");
        bsp_i2c_unlock();
        s_io = NULL; /* match the init-failed path above for every later caller */
        return;
    }
    bsp_i2c_unlock();
    ESP_LOGI(TAG, "expander up: BL_EN=EXIO2 BEE_EN=EXIO6");
}

void ui_board_backlight(bool on)
{
    if (s_io == NULL) {
        return;
    }
    if (!bsp_i2c_lock(UI_BOARD_I2C_TIMEOUT)) {
        ESP_LOGW(TAG, "backlight %s skipped: I2C bus busy", on ? "on" : "off");
        return;
    }
    esp_io_expander_set_level(s_io, PIN_BL_EN, on ? BL_ON_LEVEL : !BL_ON_LEVEL);
    bsp_i2c_unlock();
}

void ui_board_buzzer(bool on)
{
    if (s_io == NULL) {
        return;
    }
    /* Active buzzer: it self-oscillates. It has to be — BEE_EN is behind an I2C
     * expander, so there is no way to toggle it at an audio frequency. */
    if (!bsp_i2c_lock(UI_BOARD_I2C_TIMEOUT)) {
        ESP_LOGW(TAG, "buzzer %s skipped: I2C bus busy", on ? "on" : "off");
        return;
    }
    esp_io_expander_set_level(s_io, PIN_BEE_EN, on ? BEE_ON_LEVEL : !BEE_ON_LEVEL);
    bsp_i2c_unlock();
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
     * Held across the gauge read AND all three writes, and not released until
     * `out`. Unlike everything else in this file the sequence is genuinely
     * stateful -- the chip latches "unlock step 1 seen, step 2 seen" and a
     * foreign transaction in between is the one thing that could make the
     * shutdown write land on an unlocked register and silently do nothing.
     *
     * Failure aborts rather than proceeding unlocked: refusing to power off is
     * recoverable (press the button again), a half-unlocked PMIC is not.
     */
    if (!bsp_i2c_lock(UI_BOARD_I2C_TIMEOUT)) {
        ESP_LOGE(TAG, "I2C bus busy; power-off not attempted");
        (void)i2c_master_bus_rm_device(dev);
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
     * we are still here, say so: silence would look like a dead button.
     *
     * The bus lock stays held through the wait. Handing it back so the touch
     * poll can run for a board that is meant to be dead in the next few ms is
     * not worth the extra exit path; touch and the STM32 poll both already
     * tolerate a skipped turn. */
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGE(TAG, "still running 500ms after power-off; PMIC ignored it");

out:
    bsp_i2c_unlock();
    (void)i2c_master_bus_rm_device(dev);
}
