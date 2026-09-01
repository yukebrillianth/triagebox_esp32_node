#include "bsp/esp32_s3_touch_lcd_4.h"
#include "bsp/display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "asset_fs.h"
#include "tb_debug.h"
#include "tb_link_i2c.h"
#include "bp_capture.h"
#include "ui_board.h"

#include "ui.h"
#include "ui_airway.h"
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
/* Built in C, not exported from the Editor -- see ui_airway.h. */
static void show_airway(void)   { lv_screen_load(ui_airway_screen()); }
static void show_mengukur(void) { lv_screen_load(mengukur); }
static void show_result(void)   { lv_screen_load(result); }
static void show_monitor(void)  { lv_screen_load(monitor); }
static void show_test(void)     { lv_screen_load(test); }

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

/*
 * Why the app logs this itself instead of trusting the ROM's "rst:0x.." header:
 * a panic here reboots after 0 s with coredump off, so a crash and a clean boot
 * leave the same trace -- and the backlight hangs off the TCA9554, which does not
 * reset with the SoC, so a crash-reboot looks like a dead panel with the light
 * still on. That is the reported "blackscreen but backlight stays lit, kinda
 * reset". This line is the difference between guessing and knowing which of
 * panic / brownout / interrupt-WDT it is.
 *
 * Free heap goes with it because the other candidate is an LVGL allocation
 * failure: 50 tiny_ttf fonts on a clib-malloc heap, where LV_ASSERT_MALLOC ends
 * in abort() -- which arrives here next boot as PANIC.
 */
static void log_boot_reason(void)
{
    /* Indexed by esp_reset_reason_t, which is contiguous from 0. */
    static const char *const names[] = {
        "unknown", "power-on", "ext-pin", "sw-restart", "PANIC", "int-wdt",
        "task-wdt", "other-wdt", "deep-sleep", "BROWNOUT", "sdio", "usb",
        "jtag", "efuse", "PWR-GLITCH", "cpu-lockup",
    };
    esp_reset_reason_t reason = esp_reset_reason();
    const char *name = ((unsigned)reason < sizeof(names) / sizeof(names[0]))
                           ? names[reason]
                           : "out-of-range";

    ESP_LOGW(TAG, "boot: reset=%s (%d), heap free=%u min=%u", name, (int)reason,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size());
}

static void register_triage_screens(void)
{
    ui_nav_register(UI_SCREEN_HOME, show_home);
    ui_nav_register(UI_SCREEN_SCANNING, show_scanning);
    ui_nav_register(UI_SCREEN_BERHASIL, show_berhasil);
    ui_nav_register(UI_SCREEN_AGE, show_age);
    ui_nav_register(UI_SCREEN_GENDER, show_gender);
    ui_nav_register(UI_SCREEN_AIRWAY, show_airway);
    ui_nav_register(UI_SCREEN_MENGUKUR, show_mengukur);
    ui_nav_register(UI_SCREEN_RESULT, show_result);
    ui_nav_register(UI_SCREEN_MONITOR, show_monitor);
    ui_nav_register(UI_SCREEN_TEST, show_test);
}

/*
 * Heartbeat for the "blackscreen, backlight still lit, kinda reset" report. It
 * lives inside the LVGL timer callback on purpose: if this line prints, the LVGL
 * task is running and pumping timers, which splits the three candidate causes
 * apart without needing anyone to be watching the monitor at the moment it fails.
 *
 *   uptime restarts near 0   -> it WAS a reset; read the boot: reset= line above.
 *   uptime keeps climbing    -> SoC and UI are fine, the panel stream is dead
 *                               (single PSRAM framebuffer + 480x20 bounce buffer,
 *                               and CONFIG_LCD_RGB_RESTART_IN_VSYNC is not set).
 *   log stops, no boot line  -> the LVGL task itself wedged, most likely blocked
 *                               on the shared I2C bus (see the GT911 timeouts).
 *
 * lv_tick and wall clock are both printed because they can diverge: LVGL timers
 * run off lv_tick, so a starved tick source freezes the UI while the SoC is
 * healthy, and that looks identical on the panel.
 */
static void log_heartbeat(uint32_t lv_ms)
{
    ESP_LOGW(TAG, "hb: lv=%ums wall=%llums screen=%d heap=%u min=%u frames=%u",
             (unsigned)lv_ms, (unsigned long long)(esp_timer_get_time() / 1000),
             (int)ui_nav_current(), (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)tb_link_frames_ok());
}

static void runtime_timer_cb(lv_timer_t *timer)
{
    static uint32_t ticks;
    uint32_t now = lv_tick_get();

    (void)timer;
    ui_runtime_tick(now);

    /* 200 x 50 ms = 10 s: fine enough to bracket a blackout, sparse enough to
     * read a whole shift of it. WARN so it survives the default log level. */
    if ((ticks++ % 200U) == 0U) {
        log_heartbeat(now);
    }
}

void app_main(void)
{
    lv_display_t *disp;

    log_boot_reason();

    ESP_ERROR_CHECK(mount_assets());

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

    /*
     * The STM32 link shares that same I2C bus, so it cannot start any earlier
     * than this -- bsp_i2c_get_handle() is only valid once the display is up.
     * That is a change from the RS485 transport, which owned its own UART and
     * started before the display.
     *
     * Still before the first screen loads, so the Home status dots have real
     * data on their first paint. Not fatal if the STM32 is absent: polls fail,
     * the dots show the link down, and the UI runs.
     */
    if (tb_link_start() != ESP_OK) {
        ESP_LOGW(TAG, "STM32 link did not start -- UI will run without vitals");
    }

    /* The BP task feeds off the poll task's wave reads; it must exist before
     * the first measure-done (which notifies it) and is idle until then. */
    bp_capture_init();


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
