/*
 * Real board outputs: backlight + buzzer, both behind the TCA9554 expander.
 * The sim/Editor build links ui/logic/ui_board_stub.c instead.
 */
#include "ui_board.h"

#include "bsp/esp32_s3_touch_lcd_4.h"
#include "esp_io_expander.h"
#include "esp_log.h"

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
 * Polarity is NOT verified against hardware yet. Active-high is the assumption
 * (enable line, pulled low by R28 at reset). If the backlight turns out
 * inverted, flip these two and nothing else.
 */
#define BL_ON_LEVEL  1
#define BEE_ON_LEVEL 1

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
