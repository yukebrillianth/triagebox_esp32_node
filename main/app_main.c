#include "bsp/esp32_s3_touch_lcd_4.h"
#include "bsp/display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "asset_fs.h"
#include "tb_debug.h"
#include "tb_link.h"
#include "ui_board.h"

#include "ui.h"
#include "ui_input.h"
#include "ui_nav.h"
#include "ui_runtime.h"

static const char *TAG = "triagebox";

#define ASSET_MOUNT_POINT "/assets"
#define ASSET_LVGL_PATH   "A:" ASSET_MOUNT_POINT "/"

static void show_home(void)     { lv_screen_load(home); }
static void show_scanning(void) { lv_screen_load(scanning); }
static void show_berhasil(void) { lv_screen_load(berhasil); }
static void show_age(void)      { lv_screen_load(age); }
static void show_gender(void)   { lv_screen_load(gender); }
static void show_mengukur(void) { lv_screen_load(mengukur); }
static void show_result(void)   { lv_screen_load(result); }
static void show_monitor(void)  { lv_screen_load(monitor); }

static esp_err_t mount_assets(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = ASSET_MOUNT_POINT,
        .partition_label = "storage",
        .max_files = 8,
        .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    size_t total = 0;
    size_t used = 0;

    if (err != ESP_OK) {
        return err;
    }
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "assets mounted: %u/%u bytes used", (unsigned)used, (unsigned)total);
    }
    return ESP_OK;
}

static void register_triage_screens(void)
{
    ui_nav_register(UI_SCREEN_HOME, show_home);
    ui_nav_register(UI_SCREEN_SCANNING, show_scanning);
    ui_nav_register(UI_SCREEN_BERHASIL, show_berhasil);
    ui_nav_register(UI_SCREEN_AGE, show_age);
    ui_nav_register(UI_SCREEN_GENDER, show_gender);
    ui_nav_register(UI_SCREEN_MENGUKUR, show_mengukur);
    ui_nav_register(UI_SCREEN_RESULT, show_result);
    ui_nav_register(UI_SCREEN_MONITOR, show_monitor);
}

static void runtime_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_runtime_tick(lv_tick_get());
}

void app_main(void)
{
    lv_display_t *disp;

    ESP_ERROR_CHECK(mount_assets());

    /* Before ui_runtime_init(): the RX task must be draining the line before
     * the UI starts polling, or the STM32's first frames are lost. A missing
     * STM32 is not fatal — no frames simply means the UI idles on Home. */
    ESP_ERROR_CHECK(tb_link_start());

    ui_runtime_init();

    /* Default lvgl_port task_stack (7168) overflows once tiny_ttf rasterizes
     * glyphs (stb_truetype is stack-hungry) -> LoadStoreError in the scheduler. */
    bsp_display_cfg_t disp_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
    };
    disp_cfg.lvgl_port_cfg.task_stack = 32768;

    disp = bsp_display_start_with_config(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    if (!bsp_display_lock(2000)) {
        ESP_LOGE(TAG, "could not take LVGL lock");
        return;
    }

    /* Needs the I2C bus the display brought up. Enables the backlight and puts
     * the buzzer in a known-quiet state before any screen appears. */
    ui_board_init();


    asset_fs_init();
    ui_init(ASSET_LVGL_PATH);
    ui_input_keypad_init(disp);
    register_triage_screens();

    if (home == NULL) {
        ESP_LOGE(TAG, "home screen is NULL after ui_init");
        bsp_display_unlock();
        return;
    }

    lv_screen_load(home);
    lv_timer_create(runtime_timer_cb, 50, NULL);

    bsp_display_unlock();

    ESP_LOGI(TAG, "TriageBox UI up on %dx%d", (int)lv_display_get_horizontal_resolution(disp),
             (int)lv_display_get_vertical_resolution(disp));

#if CONFIG_TB_DEBUG_CONSOLE
    /* Last: the REPL takes over stdin, and it needs the UI already running so
     * injected frames land on live screens. */
    tb_debug_console_start();
#endif
}
