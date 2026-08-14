/*
 * Serial debug console: inject frames that would normally come from the STM32,
 * so the whole triage flow can be demoed before the sensor board exists.
 *
 * Build-gated by TB_DEBUG_CONSOLE (see components/triagebox_debug/CMakeLists.txt)
 * -- do not ship it enabled: it lets anyone fake patient vitals over USB.
 *
 * Type `help` in `idf.py monitor` for the command list.
 */
#include "tb_debug.h"

#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tb_link.h"
#include "tb_svm.h"
#include "tb_ui_source.h"
#include "ui_mock.h"
#include "ui_status.h"

static const char *TAG = "tb_debug";

static int cmd_rfid(int argc, char **argv)
{
    rfid_t r = {0};
    const char *tag = (argc > 1) ? argv[1] : "3021";

    strncpy(r.tag, tag, RFID_TAG_CAPACITY - 1U);
    r.present = true;
    tb_ui_source_mark_frame();
    tb_ui_source_on_rfid(&r);
    printf("injected RFID '%s'\n", r.tag);
    return 0;
}

static int cmd_vital(int argc, char **argv)
{
    /* Defaults are a healthy adult; override any prefix of the arguments. */
    vitals_t v = {
        .hr = 90, .spo2 = 98, .rr = 18, .bp_sys = 120, .bp_dia = 80,
        .battery = 80, .valid = true,
    };

    if (argc > 1) v.hr     = (uint16_t)atoi(argv[1]);
    if (argc > 2) v.spo2   = (uint16_t)atoi(argv[2]);
    if (argc > 3) v.rr     = (uint16_t)atoi(argv[3]);
    if (argc > 4) v.bp_sys = (uint16_t)atoi(argv[4]);
    if (argc > 5) v.bp_dia = (uint16_t)atoi(argv[5]);

    tb_ui_source_mark_frame();
    tb_ui_source_on_vital(&v);
    printf("injected VITAL hr=%u spo2=%u rr=%u bp=%u/%u\n",
           v.hr, v.spo2, v.rr, v.bp_sys, v.bp_dia);
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    /* No args = everything healthy, which is what a demo wants. */
    uint8_t mask = (argc > 1) ? (uint8_t)strtoul(argv[1], NULL, 0) : UI_SENSOR_ALL;
    int lora = (argc > 2) ? atoi(argv[2]) : 1;

    tb_ui_source_mark_frame();
    tb_ui_source_on_status(mask, 80, lora);
    printf("injected STATUS sensors=0x%02x lora=%d\n", mask, lora);
    return 0;
}

static int cmd_btn(int argc, char **argv)
{
    uint8_t idx = (argc > 1) ? (uint8_t)atoi(argv[1]) : 0;

    if (idx > 3U) {
        printf("button index must be 0..3\n");
        return 1;
    }
    /* Press and release: the keypad indev needs both edges. */
    tb_ui_source_on_button(idx, true);
    vTaskDelay(pdMS_TO_TICKS(120));
    tb_ui_source_on_button(idx, false);
    printf("pressed button %u\n", idx);
    return 0;
}

static int cmd_stats(int argc, char **argv)
{
    /* Time the SVM over many runs: one call is far below esp_timer's
     * resolution, so a single measurement would just read 0 or 1 us. */
    const int iterations = 1000;
    vitals_t v = {
        .hr = 112, .spo2 = 93, .rr = 24, .bp_sys = 100, .bp_dia = 65,
        .battery = 80, .valid = true,
    };
    float conf = 0.0f;
    int64_t t0;
    int64_t elapsed_us;
    /* esp_lvgl_port names it "taskLVGL", not "LVGL". */
    TaskHandle_t lvgl = xTaskGetHandle("taskLVGL");

    (void)argc;
    (void)argv;

    t0 = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        (void)tb_svm_classify(&v, &conf);
    }
    elapsed_us = esp_timer_get_time() - t0;

    printf("\n--- inference ---\n");
    printf("tb_svm_classify: %.2f us/call (%d calls in %lld us)\n",
           (double)elapsed_us / iterations, iterations, elapsed_us);
    printf("called once per patient, so ~%.4f%% of one 60 s measure window\n",
           100.0 * ((double)elapsed_us / iterations) / 60e6);

    printf("\n--- heap ---\n");
    printf("internal free  : %u bytes (min ever %u)\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    printf("PSRAM free     : %u bytes (min ever %u)\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    printf("largest block  : %u internal / %u PSRAM\n",
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

    printf("\n--- task stacks (bytes still unused; 0 means overflow) ---\n");
    if (lvgl != NULL) {
        /* This is the one that overflowed at the default 7168 during bring-up. */
        printf("taskLVGL : %u of 32768\n",
               (unsigned)(uxTaskGetStackHighWaterMark(lvgl) * sizeof(StackType_t)));
    } else {
        printf("taskLVGL : not found\n");
    }
    TaskHandle_t rx = xTaskGetHandle("tb_rx");
    if (rx != NULL) {
        printf("tb_rx    : %u of 4096\n",
               (unsigned)(uxTaskGetStackHighWaterMark(rx) * sizeof(StackType_t)));
    }

    printf("\n--- link ---\n");
    printf("frames_ok=%u crc_errors=%u\n",
           (unsigned)tb_link_frames_ok(), (unsigned)tb_link_crc_errors());
    printf("\n");
    return 0;
}

void tb_debug_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_usb_serial_jtag_config_t dev_cfg =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

    repl_cfg.prompt = "triagebox>";
    repl_cfg.max_cmdline_length = 128;

    static const esp_console_cmd_t cmds[] = {
        {.command = "rfid",   .help = "Inject RFID frame [tag]",                 .func = cmd_rfid},
        {.command = "vital",  .help = "Inject VITAL [hr spo2 rr sys dia]",       .func = cmd_vital},
        {.command = "status", .help = "Inject STATUS [sensor_mask] [lora_ok]",   .func = cmd_status},
        {.command = "btn",    .help = "Press button 0..3",                       .func = cmd_btn},
        {.command = "stats",  .help = "CPU/heap/stack report",                   .func = cmd_stats},
    };

    if (esp_console_new_repl_usb_serial_jtag(&dev_cfg, &repl_cfg, &repl) != ESP_OK) {
        ESP_LOGE(TAG, "console init failed");
        return;
    }
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGW(TAG, "debug console ON (rfid/vital/status/btn/stats) -- disable for production");
}
