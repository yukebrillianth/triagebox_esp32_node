/*
 * Host check of the RFID gate in tb_ui_source.c: after a START_SCAN, a tag is
 * believed only once the STM32 has confirmed it dropped the previous one.
 *
 * Worth a selftest because the failure mode is silent and clinical, not cosmetic:
 * the stale card belongs to the *previous* patient, so the box files one person's
 * vitals under another person's ID. Compiles the real file against test_fakes/.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fakes.h"
#include "tb_triage.h"
#include "tb_ui_source.h"
#include "ui_demo.h"
#include "ui_mock.h"
#include "ui_status.h"

/* --- stubs for everything tb_ui_source.c calls but does not own -------------- */

const char *esp_err_to_name(esp_err_t err) { (void)err; return "ESP_ERR_FAKE"; }
void vTaskDelay(int ticks) { (void)ticks; }
void ui_board_power_off(void) {}
uint32_t tb_link_frames_ok(void) { return 0; }
esp_err_t tb_link_send_cmd(uint8_t cmd) { (void)cmd; return ESP_OK; }

/* Stand in for the whole ML component: this selftest is about tb_ui_source's
 * plumbing, not the model. */
void tb_vitals_window_reset(tb_vitals_window_t *w) { (void)w; }
void tb_vitals_window_add(tb_vitals_window_t *w, const vitals_t *v)
{
    (void)v;
    if (w != NULL) {
        w->samples++;
    }
}

ui_priority_t tb_triage_classify(const tb_vitals_window_t *w, ui_age_band_t age,
                                 ui_gender_t gender, float *confidence)
{
    (void)w;
    (void)age;
    (void)gender;
    if (confidence != NULL) {
        *confidence = 0.5f;
    }
    return UI_PRIORITY_GREEN;
}

/* tb_ui_source.c reads the committed age/gender to build the feature vector. */
ui_age_band_t ui_session_get_age(void) { return UI_AGE_BAND_18_45; }
ui_gender_t ui_session_get_gender(void) { return UI_GENDER_M; }

/* The one window onto s_rfid after ui_mock_rfid_ready() has consumed the
 * one-shot flag: infer_once() hands the tag to the station through here. */
static char s_sent_tag[RFID_TAG_CAPACITY];

esp_err_t tb_link_send_result(ui_priority_t priority, float confidence,
                              const char *tag)
{
    (void)priority;
    (void)confidence;
    s_sent_tag[0] = '\0';
    if (tag != NULL) {
        snprintf(s_sent_tag, sizeof(s_sent_tag), "%s", tag);
    }
    return ESP_OK;
}

/* --- helpers ---------------------------------------------------------------- */

static void push_tag(const char *tag)
{
    rfid_t r = {0};

    snprintf(r.tag, sizeof(r.tag), "%s", tag);
    r.present = true;
    tb_ui_source_on_rfid(&r);
}

/* What tb_link_i2c.c now sends when the snapshot's rfid_len is 0. */
static void push_empty(void)
{
    const rfid_t empty = {0};

    tb_ui_source_on_rfid(&empty);
}

/* --- demo mode --------------------------------------------------------------- */

/*
 * Demo mode substitutes the feed, and the substitution happens in this file --
 * ui_demo.c only supplies the numbers. What is pinned here is the wiring: that
 * the model is not consulted, that the RFID tag is NOT faked, and that turning
 * it off returns to live data with nothing left over.
 *
 * The fake tb_triage_classify() above returns GREEN, which is what makes this
 * checkable: if demo mode ever fell through to the model, the result would be
 * GREEN instead of RED and the assert would catch it.
 */
static void test_demo_mode(void)
{
    vitals_t v;
    link_status_t st;
    const vitals_t real = {
        .hr = 72, .spo2 = 99, .rr = 14, .bp_sys = 120, .bp_dia = 78,
        .battery = 64,
        .valid_mask = UI_VITAL_HR | UI_VITAL_SPO2 | UI_VITAL_RR | UI_VITAL_BP,
        .valid = true,
    };

    ui_mock_init();
    ui_demo_set(false);
    tb_ui_source_on_vital(&real);
    tb_ui_source_on_status(UI_SENSOR_ECG, 64, 1);

    /* Baseline: live data, and the model's GREEN. */
    ui_mock_get_vitals(&v);
    assert(v.hr == 72 && v.spo2 == 99);
    assert(ui_mock_get_priority() == UI_PRIORITY_GREEN);

    /* Demo on. Same snapshot underneath, different numbers out. */
    ui_demo_set(true);
    ui_mock_get_vitals(&v);
    assert(v.hr != 72 && v.spo2 != 99);
    assert(v.valid_mask == (UI_VITAL_HR | UI_VITAL_SPO2 | UI_VITAL_RR |
                            UI_VITAL_BP));
    /* The gauge is not part of the act: a real 64% must survive. */
    assert(v.battery == 64);

    /* Fresh measurement, or infer_once() would hand back the cached GREEN. */
    push_tag("F00DF00D");
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_DEMO_PRIORITY);
    assert(ui_mock_get_confidence() == UI_DEMO_CONFIDENCE);
    /* The station still gets the patient, tag and all -- the dashboard is part
     * of what gets filmed, and the tag is the one thing demo mode never fakes. */
    assert(strcmp(s_sent_tag, "F00DF00D") == 0);

    /* Only the Sensor dot is faked; Sistem and LoRa keep reporting the link. */
    ui_mock_get_link_status(&st);
    assert(st.sensor_mask == UI_SENSOR_ALL);

    /* Off again: live data returns, with no demo residue. */
    ui_demo_set(false);
    ui_mock_get_vitals(&v);
    assert(v.hr == 72 && v.spo2 == 99);
    ui_mock_get_link_status(&st);
    assert(st.sensor_mask == UI_SENSOR_ECG);
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_PRIORITY_GREEN);
}

int main(void)
{
    rfid_t out;

    test_demo_mode();

    ui_mock_init();

    /* First scan of the session: nothing to clear, so no waiting. */
    assert(!ui_mock_rfid_ready(&out));
    push_tag("AABBCCDD");
    assert(ui_mock_rfid_ready(&out));
    assert(strcmp(out.tag, "AABBCCDD") == 0 && out.present);
    assert(!ui_mock_rfid_ready(&out)); /* one-shot */

    /*
     * Restart. The STM32 clears its published tag when it services the command,
     * which is up to a superloop later -- so these next polls still carry the old
     * card. Accepting one is the bug: "already has a value despite no card".
     */
    ui_mock_start_scan();
    push_tag("AABBCCDD");
    push_tag("AABBCCDD");
    assert(!ui_mock_rfid_ready(&out));

    /* Confirmation arrives, and only now do tags count again. */
    push_empty();
    assert(!ui_mock_rfid_ready(&out)); /* an empty snapshot is not a scan */
    push_tag("11223344");
    assert(ui_mock_rfid_ready(&out));
    assert(strcmp(out.tag, "11223344") == 0);

    /* Re-presenting the SAME card after a Restart must still register: the gate
     * is armed by the command, not by comparing tag strings. */
    ui_mock_start_scan();
    push_empty();
    push_tag("11223344");
    assert(ui_mock_rfid_ready(&out));
    assert(strcmp(out.tag, "11223344") == 0);

    /*
     * Empty snapshots after a successful scan must not erase the ID: Monitor and
     * Result read it for the rest of the session, and losing it mid-triage would
     * blank the patient identity on screen.
     */
    push_empty();
    push_empty();
    (void)ui_mock_get_priority(); /* runs infer_once(), which reports the tag */
    assert(strcmp(s_sent_tag, "11223344") == 0);

    /* A failed START_SCAN write still gates. A scan that never completes is a
     * visible fault; one that completes with the wrong identity is not. */
    ui_mock_init();
    push_tag("DEADBEEF");
    assert(ui_mock_rfid_ready(&out));
    ui_mock_start_scan();
    push_tag("DEADBEEF");
    assert(!ui_mock_rfid_ready(&out));

    printf("tb_ui_source: tags ignored until an rfid_len==0 snapshot confirms "
           "START_SCAN; a good tag survives later empty snapshots; demo mode "
           "swaps the vitals and the verdict but never the tag\n");
    return 0;
}
