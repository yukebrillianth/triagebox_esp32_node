/*
 * Device-side ui_mock.h: RS485 frames in, ui/logic/ reads it unchanged.
 * See tb_ui_source.h for why this file exists instead of an #ifdef.
 */
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tb_link.h"
#include "tb_svm.h"
#include "tb_ui_source.h"
#include "ui_mock.h"

static const char *TAG = "tb_ui_src";

/* RX task writes these, the LVGL task reads them. Copies are a few words, so a
 * spinlock beats a mutex here — and it is legal from both contexts. */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static vitals_t s_vitals;
static bool s_have_vitals;

static rfid_t s_rfid;
static bool s_rfid_ready;

static bool s_btn_pending;
static btn_event_t s_btn;

static uint8_t s_sensor_mask;

/* Measure window state, owned by the LVGL task only — no lock needed. */
static uint32_t s_now_ms;
static bool s_measure_active;
static bool s_measure_done;
static uint32_t s_measure_start_ms;

/* Inference result, computed once per measurement. */
static bool s_have_priority;
static ui_priority_t s_priority;
static float s_confidence;

/* ---------------------------------------------------------------- RX task -- */

void tb_ui_source_on_vital(const vitals_t *v)
{
    if (v == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    s_vitals = *v;
    s_have_vitals = true;
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_on_button(uint8_t index, bool pressed)
{
    /* Single slot, same as the mock: the UI drains it every 50 ms and a human
     * cannot press faster. ponytail: ceiling is one event per LVGL tick —
     * switch to a FreeRTOS queue if the STM32 ever bursts BUTTON frames. */
    if (index > 3U) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    s_btn.index = index;
    s_btn.pressed = pressed;
    s_btn.timestamp_ms = s_now_ms;
    s_btn_pending = true;
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_on_rfid(const rfid_t *r)
{
    if (r == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    s_rfid = *r;
    s_rfid_ready = r->present;
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_on_status(uint8_t sensor_ok_mask, uint8_t battery)
{
    portENTER_CRITICAL(&s_mux);
    s_sensor_mask = sensor_ok_mask;
    /* Battery also rides on VITAL, but STATUS keeps arriving while idle. */
    s_vitals.battery = battery;
    portEXIT_CRITICAL(&s_mux);
}

uint8_t tb_ui_source_sensor_mask(void)
{
    return s_sensor_mask;
}

/* ------------------------------------------------------------- ui_mock.h -- */

void ui_mock_init(void)
{
    portENTER_CRITICAL(&s_mux);
    memset(&s_vitals, 0, sizeof(s_vitals));
    s_have_vitals = false;
    memset(&s_rfid, 0, sizeof(s_rfid));
    s_rfid_ready = false;
    s_btn_pending = false;
    memset(&s_btn, 0, sizeof(s_btn));
    s_sensor_mask = 0;
    portEXIT_CRITICAL(&s_mux);

    s_now_ms = 0;
    s_measure_active = false;
    s_measure_done = false;
    s_measure_start_ms = 0;
    s_have_priority = false;
    s_priority = UI_PRIORITY_BLACK;
    s_confidence = 0.0f;
}

void ui_mock_tick(uint32_t now_ms)
{
    s_now_ms = now_ms;

    if (s_measure_active && !s_measure_done) {
        if ((now_ms - s_measure_start_ms) >= UI_MEASURE_MS) {
            s_measure_done = true;
            s_measure_active = false;
        }
    }
}

void ui_mock_start_scan(void)
{
    portENTER_CRITICAL(&s_mux);
    s_rfid_ready = false;
    memset(&s_rfid, 0, sizeof(s_rfid));
    portEXIT_CRITICAL(&s_mux);

    if (tb_link_send_cmd(TB_CMD_START_SCAN) != ESP_OK) {
        ESP_LOGW(TAG, "START_SCAN not sent");
    }
}

bool ui_mock_rfid_ready(rfid_t *out)
{
    bool ready;

    portENTER_CRITICAL(&s_mux);
    ready = s_rfid_ready;
    if (ready) {
        if (out != NULL) {
            *out = s_rfid;
        }
        s_rfid_ready = false; /* one-shot, same contract as the mock */
    }
    portEXIT_CRITICAL(&s_mux);

    return ready;
}

void ui_mock_start_measure(void)
{
    s_measure_active = true;
    s_measure_done = false;
    s_measure_start_ms = s_now_ms;
    s_have_priority = false;

    if (tb_link_send_cmd(TB_CMD_START_MEASURE) != ESP_OK) {
        ESP_LOGW(TAG, "START_MEASURE not sent");
    }
}

uint8_t ui_mock_measure_progress(void)
{
    uint32_t elapsed;

    if (s_measure_done) {
        return 100;
    }
    if (!s_measure_active) {
        return 0;
    }
    elapsed = s_now_ms - s_measure_start_ms;
    if (elapsed >= UI_MEASURE_MS) {
        return 100;
    }
    return (uint8_t)((elapsed * 100U) / UI_MEASURE_MS);
}

bool ui_mock_measure_done(void)
{
    return s_measure_done;
}

void ui_mock_get_vitals(vitals_t *out)
{
    if (out == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    *out = s_vitals;
    /* No VITAL frame yet means no readings — the screens render "--" on
     * !valid, so never claim validity we do not have. */
    out->valid = s_have_vitals && s_vitals.valid;
    portEXIT_CRITICAL(&s_mux);
}

/* Runs the SVM once per measurement and reports the result to the STM32,
 * which owns the LoRa TX. ui_runtime.c calls this exactly once via
 * pull_mock_priority_once(). */
static void infer_once(void)
{
    vitals_t v;
    char tag[RFID_TAG_CAPACITY];

    if (s_have_priority) {
        return;
    }

    ui_mock_get_vitals(&v);
    s_priority = tb_svm_classify(&v, &s_confidence);
    s_have_priority = true;

    portENTER_CRITICAL(&s_mux);
    memcpy(tag, s_rfid.tag, sizeof(tag));
    portEXIT_CRITICAL(&s_mux);
    tag[RFID_TAG_CAPACITY - 1U] = '\0';

    ESP_LOGI(TAG, "svm: priority=%d confidence=%.2f valid=%d", (int)s_priority,
             (double)s_confidence, (int)v.valid);

    if (tb_link_send_result(s_priority, s_confidence, tag[0] ? tag : NULL) != ESP_OK) {
        ESP_LOGW(TAG, "RESULT not sent — station will miss this triage");
    }
}

ui_priority_t ui_mock_get_priority(void)
{
    infer_once();
    return s_priority;
}

float ui_mock_get_confidence(void)
{
    infer_once();
    return s_confidence;
}

const char *ui_mock_get_reasons(void)
{
    /* An SVM has no rule path to quote, and no separate threshold table was
     * added, so the backend gets the empty list it defaults to. */
    return "";
}

void ui_mock_cycle_priority(void)
{
    /* QA-only shortcut in the mock; on hardware the SVM decides. */
}

void ui_mock_push_button(uint8_t index, bool pressed)
{
    tb_ui_source_on_button(index, pressed);
}

bool ui_mock_pop_button(btn_event_t *out)
{
    bool pending;

    portENTER_CRITICAL(&s_mux);
    pending = s_btn_pending;
    if (pending) {
        if (out != NULL) {
            *out = s_btn;
        }
        s_btn_pending = false;
    }
    portEXIT_CRITICAL(&s_mux);

    return pending;
}
