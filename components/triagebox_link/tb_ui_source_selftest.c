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
uint32_t tb_link_crc_errors(void) { return 0; }
esp_err_t tb_link_send_cmd(uint8_t cmd) { (void)cmd; return ESP_OK; }
esp_err_t tb_link_send_bp(uint16_t sys, uint16_t dia)
{
    (void)sys; (void)dia;
    return ESP_OK;
}

/*
 * The real band -> years conversion, NOT a copy of it: age travels as the
 * mid-point the model scored (53 for 46..60), and a table transcribed here would
 * happily agree with a wrong one in the firmware -- which is how a sign error in
 * this repo's biquads survived every test for weeks. tb_triage_classify() is not
 * in this file (it lives in tb_triage_model.c) so the stub above still stands.
 *
 * ponytail: included rather than linked because tools/run_selftests.sh's link
 * line for this test belongs to someone else. Add tb_triage.c there and delete
 * this line -- the duplicate symbol will say so loudly rather than silently.
 */
#include "../triagebox_ml/tb_triage.c"

/* bp_capture hooks are host-inert here, but measure_done is COUNTED: an abort
 * that forgets it leaves the 124-byte wave read running every 50 ms for the rest
 * of the session, and nothing on screen says so. */
static unsigned s_capture_done_calls;

void bp_capture_start(void) {}
void bp_capture_measure_done(void) { ++s_capture_done_calls; }

/* The measure-to-verdict timing log reads this; monotonic but not advanced by
 * these tests, which are about the RFID gate and the HR source rather than the
 * clock. tb_fake_time_set() exists so a future timing test can move it. */
static int64_t s_fake_us = 1000;
int64_t tb_fake_time_us(void) { return s_fake_us; }
void tb_fake_time_set(int64_t us) { s_fake_us = us; }

/* Stand in for the whole ML component: this selftest is about tb_ui_source's
 * plumbing, not the model. GREEN/esi 3 by default, which is what makes the
 * demo-mode test below work -- if demo mode ever fell through to the model, the
 * result would be GREEN instead of RED and the assert would catch it. A test can
 * script the refusal (BLACK with esi 0) that tb_classify() returns when a vital
 * is missing. */
static ui_priority_t s_model_priority = UI_PRIORITY_GREEN;
static int s_model_esi = 3;

ui_priority_t tb_triage_classify(const vitals_t *v, ui_age_band_t age,
                                 ui_gender_t gender, bool airway_problem,
                                 float *confidence, int *esi)
{
    (void)v;
    (void)age;
    (void)gender;
    (void)airway_problem;
    if (confidence != NULL) {
        /* The real one reports 0 with a refusal: there is no winning class. */
        *confidence = (s_model_esi == 0) ? 0.0f : 0.5f;
    }
    if (esi != NULL) {
        *esi = s_model_esi;
    }
    return s_model_priority;
}

/* tb_ui_source.c reads the committed age/gender to build the feature vector, and
 * now also to tell the STM32 what the model was scored with -- so these are
 * scriptable, including the "never answered" half. Defaults are a 31-year-old
 * male with no RR typed, which is what the older tests in this file assume.
 *
 * There is deliberately no ui_session_has_gender() here, because tb_ui_source.c
 * deliberately does not call it: a never-answered gender IS UI_GENDER_U in
 * ui_session.c (its reset default, and what set_gender() clears has_gender for),
 * and U and UNKNOWN both have to go out as 0. */
static bool s_has_age = true;
static ui_age_band_t s_age = UI_AGE_BAND_18_45;
static ui_gender_t s_gender = UI_GENDER_M;
static bool s_has_rr;
static ui_rr_band_t s_rr = UI_RR_BAND_12_20;

bool ui_session_has_age(void) { return s_has_age; }
ui_age_band_t ui_session_get_age(void) { return s_age; }
ui_gender_t ui_session_get_gender(void) { return s_gender; }
bool ui_session_get_airway(void) { return false; }
/* No RR typed in by default: ui_mock_get_vitals() must then leave rr alone,
 * which is what keeps the older tests measuring the RFID gate rather than the RR
 * overlay. */
bool ui_session_has_rr(void) { return s_has_rr; }
ui_rr_band_t ui_session_get_rr(void) { return s_rr; }

/* The one window onto s_rfid after ui_mock_rfid_ready() has consumed the
 * one-shot flag: infer_once() hands the tag to the station through here. */
static char s_sent_tag[RFID_TAG_CAPACITY];
/* What the last infer_once() told the STM32, so the verdict tests can read it.
 * sent_none means the priority register got TB_PRIORITY_WIRE_NONE, which the
 * stub cannot see a ui_priority_t for. */
static int s_sent_priority = -1;
static float s_sent_confidence = -1.0f;
static bool s_sent_none;
/* The four patient bytes, and the ordering stamp: the slave latches the verdict
 * complete on CONFIDENCE, so the patient group -- ESI included -- must reach the
 * registers BEFORE the verdict write. Each stub takes the next number from one
 * shared counter, so "patient before verdict" is an assert, not a reading of the
 * call order in the file under test. */
static unsigned s_wire_seq;
static unsigned s_patient_seq;
static unsigned s_result_seq;
static uint8_t s_sent_rr;
static uint8_t s_sent_age;
static uint8_t s_sent_gender;
static uint8_t s_sent_esi;

esp_err_t tb_link_send_patient(uint8_t rr, uint8_t age_years,
                               uint8_t gender_ascii, uint8_t esi)
{
    s_sent_rr = rr;
    s_sent_age = age_years;
    s_sent_gender = gender_ascii;
    s_sent_esi = esi;
    s_patient_seq = ++s_wire_seq;
    return ESP_OK;
}

esp_err_t tb_link_send_result(ui_priority_t priority, float confidence,
                              const char *tag)
{
    s_sent_none = false;
    s_sent_priority = (int)priority;
    s_sent_confidence = confidence;
    s_sent_tag[0] = '\0';
    if (tag != NULL) {
        snprintf(s_sent_tag, sizeof(s_sent_tag), "%s", tag);
    }
    s_result_seq = ++s_wire_seq;
    return ESP_OK;
}

/* tb_link_i2c.c's refusal path. The real one writes TB_PRIORITY_WIRE_NONE
 * (0xFF) to the priority register; the register write itself is hardware, so
 * what the host test pins is WHICH function each case picks. */
esp_err_t tb_link_send_unscored(void)
{
    s_sent_none = true;
    s_sent_priority = -1;
    s_sent_confidence = 0.0f;
    s_sent_tag[0] = '\0';
    s_result_seq = ++s_wire_seq;
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

/*
 * Push a tag AND consume it, which is what the Scanning screen does through
 * ui_runtime_tick(). The consume is where this patient's identity is latched, so
 * a test that only pushes has scanned nobody.
 */
static void scan_tag(const char *tag)
{
    push_tag(tag);
    assert(ui_mock_rfid_ready(NULL));
}

/* --- demo mode --------------------------------------------------------------- */

/*
 * Demo mode substitutes the feed, and the substitution happens in this file --
 * ui_demo.c only supplies the numbers. What is pinned here is the wiring: that
 * the model is not consulted, that the RFID tag is NOT faked, that the mode ends
 * with the session (a take must not leak into the next patient), and that
 * turning it off returns to live data with nothing left over.
 *
 * The fake tb_triage_classify() above answers GREEN/esi 3 by default, which is
 * what makes this checkable: if demo mode ever fell through to the model, the
 * result would be GREEN instead of RED and the assert would catch it.
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
    /* mark_frame() with the status, as poll_once() does: the sensor mask is only
     * as trustworthy as the link that delivered it, so without this the dot
     * correctly reads ERROR and this test would be measuring the wrong thing. */
    tb_ui_source_mark_frame();
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
    scan_tag("F00DF00D");
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_DEMO_PRIORITY);
    assert(ui_mock_get_confidence() == UI_DEMO_CONFIDENCE);
    /* The station still gets the patient, tag and all, and the verdict still
     * travels as RED -- the dashboard is part of what gets filmed. But the
     * confidence byte is 0.00, not the 0.93 on screen: 0.93 would read on the
     * backend as a measurement, and demo mode measures nothing. */
    assert(!s_sent_none && s_sent_priority == (int)UI_PRIORITY_RED);
    assert(strcmp(s_sent_tag, "F00DF00D") == 0);
    assert(s_sent_confidence == 0.0f);

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
    assert(!s_sent_none && s_sent_priority == (int)UI_PRIORITY_GREEN);
    /* Back on the wire: a real verdict carries the real confidence. */
    assert(s_sent_confidence == 0.5f);

    /* And the mode ends with the session it was filmed in. Without this, the
     * next patient -- a real one -- is shown and reported RED on the previous
     * take's leftovers. */
    ui_demo_set(true);
    assert(ui_demo_enabled());
    ui_mock_end_session();
    assert(!ui_demo_enabled());
    ui_mock_init();
}

/* --- ECG-first heart rate ---------------------------------------------------- */

/*
 * The finger clip is the unreliable sensor on this board revision, so a
 * PPG-sourced rate must not displace a recent ECG one -- and must still be used
 * once the ECG one is stale, because tb_classify() refuses to score on hr <= 0
 * and blanking would cost the whole verdict.
 *
 * Pinned rather than eyeballed because both failure modes are silent: too eager
 * and the tile freezes on a dead number for the next patient, too shy and it
 * flickers between two sensors several times a minute with no way to tell which
 * is on screen.
 */
static void push_hr(uint16_t hr, bool from_ppg)
{
    const vitals_t v = {
        .hr = hr, .spo2 = 97, .hr_from_ppg = from_ppg,
        .valid_mask = UI_VITAL_HR | UI_VITAL_SPO2, .valid = true,
    };

    tb_ui_source_on_vital(&v);
}

static void test_hr_prefers_ecg(void)
{
    vitals_t v;

    ui_mock_init();
    ui_demo_set(false);

    /* No ECG has ever arrived: a finger rate is all there is, so it passes
     * through untouched -- label and all. */
    ui_mock_tick(1000U);
    push_hr(120U, true);
    ui_mock_get_vitals(&v);
    assert(v.hr == 120U && v.hr_from_ppg);

    /* An ECG rate arrives and is adopted. */
    push_hr(82U, false);
    ui_mock_get_vitals(&v);
    assert(v.hr == 82U && !v.hr_from_ppg);

    /* Finger clip twitches: 140 from the PPG must not reach the screen while
     * the ECG's 82 is still recent, and the label must say EKG because the
     * number IS the ECG's. */
    ui_mock_tick(4000U);
    push_hr(140U, true);
    ui_mock_get_vitals(&v);
    assert(v.hr == 82U && !v.hr_from_ppg);

    /* Past the hold, the ECG number is history and the live pulse rate wins --
     * honestly labelled, so the operator sees the source change. */
    ui_mock_tick(1000U + 11000U);
    push_hr(140U, true);
    ui_mock_get_vitals(&v);
    assert(v.hr == 140U && v.hr_from_ppg);

    /* A fresh ECG reading re-arms the hold rather than leaving it expired. */
    push_hr(75U, false);
    push_hr(150U, true);
    ui_mock_get_vitals(&v);
    assert(v.hr == 75U && !v.hr_from_ppg);

    /* And an absent HR is left absent: nothing to prefer, nothing to invent. */
    {
        const vitals_t spo2_only = {
            .spo2 = 96, .valid_mask = UI_VITAL_SPO2,
        };

        tb_ui_source_on_vital(&spo2_only);
        ui_mock_get_vitals(&v);
        assert((v.valid_mask & UI_VITAL_HR) == 0U && v.hr == 0U);
    }
}

/*
 * The BP gate: bp_capture.c refuses to publish a prediction from a window with
 * no ECG rate in it, because the model's features are pulse arrival times
 * measured from the R wave and there is no reference instant without electrodes.
 *
 * Worth pinning because the observed failure was NOT a refusal: with only the
 * finger clip on, one run published 113/59 and the next published nothing, from
 * identically absent electrodes. Amplitude gating cannot separate those -- an
 * open AD8232 input carries mains hum -- so the flag has to be the test, and it
 * has to be scoped to one measurement window rather than to the session.
 */
static void test_ecg_gate_is_per_window(void)
{
    ui_mock_init();
    ui_demo_set(false);

    /* Nothing yet: a window that has not started has seen no ECG. */
    assert(!tb_ui_source_ecg_rate_seen());

    /* Finger clip only, for a whole window: still no ECG. */
    tb_ui_source_bp_arm();
    push_hr(120U, true);
    push_hr(118U, true);
    assert(!tb_ui_source_ecg_rate_seen());

    /* One ECG-sourced poll anywhere in the window is enough -- the electrodes
     * were on the patient, which is what the gate is asking. */
    push_hr(82U, false);
    assert(tb_ui_source_ecg_rate_seen());
    push_hr(140U, true);
    assert(tb_ui_source_ecg_rate_seen());

    /* Next measurement starts clean. Without this the first patient's electrodes
     * would authorise every later patient's BP. */
    tb_ui_source_bp_arm();
    assert(!tb_ui_source_ecg_rate_seen());
    /* Note the display hold is deliberately NOT reset by arming: 82 still shows
     * while it is fresh, because a tile holding a 4 s-old rate is not the same
     * claim as publishing a blood pressure. */
    {
        vitals_t v;

        push_hr(150U, true);
        ui_mock_get_vitals(&v);
        assert(v.hr == 82U && !v.hr_from_ppg);
        assert(!tb_ui_source_ecg_rate_seen());
    }
}

/*
 * The gate's deadline, which is the fix for "kadang RFID ga detect".
 *
 * The gate opens on an rfid_len == 0 snapshot, and the STM32 publishes one only
 * if its ServiceRfid() pass found nothing. But its superloop order is
 * take_cmd -> ServiceRfid -> publish, so a card ALREADY sitting on the reader
 * when START_SCAN lands is cleared and re-found inside the same pass: the empty
 * snapshot never happens and, without a deadline, every subsequent tag is
 * rejected for the rest of the session. One pass is bounded well under 500 ms
 * (RFID scan ~120 ms + LoRa reply ~240 ms + DSP), so past RFID_GATE_MAX_MS a tag
 * in the snapshot was necessarily found after the clear.
 */
static void test_gate_deadline(void)
{
    rfid_t out;

    ui_mock_init();
    ui_mock_tick(100000U); /* well past 0, so wrap-safe subtraction is exercised */

    /* First patient, accepted with the gate open. */
    push_tag("AABBCCDD");
    assert(ui_mock_rfid_ready(&out));

    /* Restart with the card still on the reader: no empty snapshot will ever
     * come, and inside the deadline the tag is still refused. */
    ui_mock_start_scan();
    ui_mock_tick(100500U);
    push_tag("AABBCCDD");
    push_tag("AABBCCDD");
    assert(!ui_mock_rfid_ready(&out));

    /* Past the deadline the same card registers -- one superloop pass has
     * provably elapsed, so this reading is post-clear. */
    ui_mock_tick(101200U);
    push_tag("AABBCCDD");
    assert(ui_mock_rfid_ready(&out));
    assert(strcmp(out.tag, "AABBCCDD") == 0);

    /* And an empty snapshot still opens the gate immediately when one does
     * arrive -- the deadline is a fallback, not a replacement. */
    ui_mock_start_scan();
    ui_mock_tick(101300U);
    push_empty();
    push_tag("11223344");
    assert(ui_mock_rfid_ready(&out));
    assert(strcmp(out.tag, "11223344") == 0);
}

/* --- what reaches the station ------------------------------------------------ */

/*
 * A REFUSAL MUST NOT BE PUBLISHED AS A TRIAGE, and this is the highest-value
 * assert in the file.
 *
 * tb_classify() answers BLACK with esi 0 when a feature is missing -- it says so
 * in tb_classify.h -- and BLACK on the wire is 0, which the station's
 * lora_vital_priority_name() turns into "BLACK" and publishes as EXPECTANT. So
 * the patient whose finger clip fell off was filed as dead, with an alarm. The
 * fix is one byte, TB_PRIORITY_WIRE_NONE, and the reason it needs a test is that
 * both paths look identical from the screen: BLACK is BLACK either way.
 */
static void test_refusal_is_not_published_as_black(void)
{
    ui_mock_init();
    ui_demo_set(false);
    scan_tag("C0FFEE01");

    /* A real BLACK -- the model looked at a patient and said EXPECTANT -- still
     * travels as a verdict. Asserted first so the refusal below cannot pass by
     * withholding everything. */
    s_model_priority = UI_PRIORITY_BLACK;
    s_model_esi = 1;
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_PRIORITY_BLACK);
    assert(!s_sent_none && s_sent_priority == (int)UI_PRIORITY_BLACK);
    assert(strcmp(s_sent_tag, "C0FFEE01") == 0);

    /* The refusal: same colour, esi 0. Nothing about it may reach the station as
     * a level -- and the tag goes nowhere either, since there is no verdict to
     * attach to it. */
    s_model_esi = 0;
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_PRIORITY_BLACK); /* the screen's problem */
    assert(ui_mock_get_esi() == 0);
    assert(s_sent_none);

    s_model_priority = UI_PRIORITY_GREEN;
    s_model_esi = 3;
}

/*
 * The four bytes the STM32 cannot measure, and the order they reach it in.
 *
 * Every one of them has a silent wrong answer that looks right on screen: the
 * band INDEX instead of the age in years (2 rather than 53), 'U' or a fabricated
 * 'M' for a sex nobody entered, the collapsed colour instead of the raw ESI
 * (GREEN is 3, 4 AND 5), and -- worst -- the ESI arriving after CONFIDENCE has
 * already latched the verdict, which publishes the PREVIOUS patient's severity
 * under this patient's colour.
 *
 * Values are asserted against the shipped constants, not re-derived: the real
 * ui_rr_band_value() and tb_triage_age_years() are linked in above, so a change
 * to either table has to be made here too, deliberately.
 */
static void forget_sends(void)
{
    s_wire_seq = 0U;
    s_patient_seq = 0U;
    s_result_seq = 0U;
    s_sent_rr = 0xEEU;
    s_sent_age = 0xEEU;
    s_sent_gender = 0xEEU;
    s_sent_esi = 0xEEU;
}

static void test_patient_fields_reach_the_wire(void)
{
    ui_mock_init();
    ui_demo_set(false);
    scan_tag("5E5E5E5E");

    /* A fully answered patient: 46..60, female, 21..30 breaths/min. */
    s_has_age = true;
    s_age = UI_AGE_BAND_46_60;
    s_gender = UI_GENDER_F;
    s_has_rr = true;
    s_rr = UI_RR_BAND_21_30;
    s_model_priority = UI_PRIORITY_GREEN;
    s_model_esi = 4;

    forget_sends();
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_PRIORITY_GREEN);

    /* Ordering first, because it is the one that cannot be seen anywhere else. */
    assert(s_patient_seq != 0U && s_result_seq != 0U);
    assert(s_patient_seq < s_result_seq);

    /* RR is the operator's typed band, which is also the rr the model was
     * scored with -- ui_mock_get_vitals() substitutes the same call. */
    assert(s_sent_rr == 25U && ui_rr_band_value(UI_RR_BAND_21_30) == 25U);
    /* YEARS, and the band index for 46..60 is 2, so this fails loudly if the
     * index is ever sent instead. */
    assert(s_sent_age == 53U && tb_triage_age_years(UI_AGE_BAND_46_60) == 53.0f);
    assert((ui_age_band_t)2 == UI_AGE_BAND_46_60 && s_sent_age != 2U);
    /* ui_gender_t's enumerators ARE the ASCII codes. */
    assert(s_sent_gender == (uint8_t)'F');
    /* The raw ESI, not the colour: 3, 4 and 5 are all GREEN, so the 4 here is
     * the only place a could-deteriorate patient is distinguishable. */
    assert(s_sent_esi == 4U);

    /* Nothing answered: 0 in all three, never a plausible default. An 18..45
     * male at 16 breaths/min is exactly the patient a fabricated default would
     * invent, and it would be wrong every time silently. */
    s_has_age = false;
    s_gender = UI_GENDER_U;
    s_has_rr = false;
    forget_sends();
    ui_mock_start_measure();
    (void)ui_mock_get_priority();
    assert(s_sent_rr == 0U && s_sent_age == 0U && s_sent_gender == 0U);
    assert(s_patient_seq < s_result_seq);

    /* A refusal still writes the group, esi 0 and all, and still writes it
     * first. Harmless -- TB_PRIORITY_WIRE_NONE makes the station withhold the
     * whole vital -- but it must not turn the refusal into something
     * publishable, so the verdict write is still the unscored one. */
    s_has_age = true;
    s_age = UI_AGE_BAND_6_17;
    s_gender = UI_GENDER_M;
    s_has_rr = true;
    s_rr = UI_RR_BAND_OVER_30;
    s_model_esi = 0;
    forget_sends();
    ui_mock_start_measure();
    (void)ui_mock_get_priority();
    assert(s_sent_none);
    assert(s_sent_esi == 0U);
    assert(s_patient_seq < s_result_seq);
    /* The two bands at the ends of their tables, so the whole range is pinned as
     * fitting the one wire byte each register has. */
    assert(s_sent_rr == 36U && s_sent_age == 12U);

    /* Demo mode: the operator's real answers travel unchanged and the ESI matches
     * the colour being filmed, because a RED beside esi 0 is a record no real run
     * produces. The zeroed confidence stays the single marker. */
    s_model_esi = 3;
    ui_demo_set(true);
    forget_sends();
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_DEMO_PRIORITY);
    assert(s_sent_esi == (uint8_t)UI_DEMO_ESI);
    assert(s_sent_rr == 36U && s_sent_age == 12U && s_sent_gender == (uint8_t)'M');
    assert(s_sent_confidence == 0.0f);
    assert(s_patient_seq < s_result_seq);

    ui_demo_set(false);
    s_has_age = true;
    s_age = UI_AGE_BAND_18_45;
    s_gender = UI_GENDER_M;
    s_has_rr = false;
    s_rr = UI_RR_BAND_12_20;
    s_model_priority = UI_PRIORITY_GREEN;
    s_model_esi = 3;
}

/*
 * The verdict is filed under the tag THIS patient was scanned with, not whatever
 * the reader saw last.
 *
 * Every poll overwrites the live tag once the gate is open, so a second card
 * brushed against the reader during a measurement used to send the result under
 * the wrong ID -- the same class of failure the RFID gate exists to prevent, and
 * silent, because the screen kept showing the right patient the whole time.
 */
static void test_verdict_uses_the_scanned_tag(void)
{
    ui_mock_init();
    ui_demo_set(false);
    scan_tag("A1A1A1A1");

    ui_mock_start_measure();
    /* Someone else's card, mid-measurement. The gate is open (this patient's scan
     * completed), so the live copy legitimately takes it. */
    push_tag("B2B2B2B2");

    assert(ui_mock_get_priority() == UI_PRIORITY_GREEN);
    assert(strcmp(s_sent_tag, "A1A1A1A1") == 0);

    /* A restarted scan voids the identity even without passing Home -- BERHASIL's
     * "Restart" does exactly that. Until a new card is scanned there is no tag to
     * file anything under, so the verdict goes out with none rather than with the
     * previous patient's. */
    ui_mock_start_scan();
    ui_mock_start_measure();
    assert(ui_mock_get_priority() == UI_PRIORITY_GREEN);
    assert(s_sent_tag[0] == '\0');
}

/*
 * An abort must close the waveform capture, and must not fire the BP task when
 * there was no capture to close.
 *
 * Left running, s_capturing stays true and poll_wave_if_due() keeps issuing a
 * ~25 ms transaction every 50 ms on the bus the GT911 touch panel shares, for the
 * rest of the session. Fired spuriously, the BP task re-scores the PREVIOUS
 * window still sitting in its accumulator and republishes that pressure for the
 * patient who just left.
 */
static void test_abort_closes_the_capture(void)
{
    ui_mock_init();
    s_capture_done_calls = 0U;

    /* Nothing running: end_session must not wake the BP task. This is also the
     * boot path -- ui_runtime_init() navigates to Home. */
    ui_mock_end_session();
    assert(s_capture_done_calls == 0U);

    /* Aborted mid-measurement: exactly one close. */
    ui_mock_start_measure();
    ui_mock_end_session();
    assert(s_capture_done_calls == 1U);

    /* A window that ran to completion closed itself in ui_mock_tick(), so the
     * end_session that follows the operator back to Home must not close it
     * twice. */
    ui_mock_start_measure();
    ui_mock_tick(UI_MEASURE_MS + 1U);
    assert(ui_mock_measure_done());
    assert(s_capture_done_calls == 2U);
    ui_mock_end_session();
    assert(s_capture_done_calls == 2U);
}

/*
 * The Sensor dot ages with the link.
 *
 * The mask is a snapshot field: with the STM32 gone, no poll updates it and the
 * dot stayed green on the last mask the dead board ever sent. Zeroing it past
 * LINK_STALE_MS is enough -- ui_status_sensors(0) already reports ERROR.
 */
static void test_sensor_mask_goes_stale(void)
{
    link_status_t st;

    ui_mock_init();
    ui_demo_set(false);

    /* Before the first frame there is nothing to age and nothing to claim. */
    ui_mock_get_link_status(&st);
    assert(st.link_never_seen && st.sensor_mask == 0U);

    ui_mock_tick(10000U);
    tb_ui_source_mark_frame();
    tb_ui_source_on_status(UI_SENSOR_ECG | UI_SENSOR_MAX30102, 80, 1);
    ui_mock_get_link_status(&st);
    assert(st.sensor_mask == (UI_SENSOR_ECG | UI_SENSOR_MAX30102));

    /* One missed STATUS is still alive: the same 45 s the Sistem dot allows. */
    ui_mock_tick(10000U + 44000U);
    ui_mock_get_link_status(&st);
    assert(st.sensor_mask == (UI_SENSOR_ECG | UI_SENSOR_MAX30102));

    /* Past it the board is gone, and a mask from a board that stopped talking
     * describes its last breath rather than its health. */
    ui_mock_tick(10000U + 46000U);
    ui_mock_get_link_status(&st);
    assert(st.sensor_mask == 0U);

    /* A frame revives it without a new STATUS: the mask is still the one the
     * STM32 published, it is only its age that was the problem. */
    tb_ui_source_mark_frame();
    ui_mock_get_link_status(&st);
    assert(st.sensor_mask == (UI_SENSOR_ECG | UI_SENSOR_MAX30102));
}

int main(void)
{
    rfid_t out;

    test_demo_mode();
    test_hr_prefers_ecg();
    test_ecg_gate_is_per_window();
    test_gate_deadline();
    test_refusal_is_not_published_as_black();
    test_patient_fields_reach_the_wire();
    test_verdict_uses_the_scanned_tag();
    test_abort_closes_the_capture();
    test_sensor_mask_goes_stale();

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
           "START_SCAN (or the gate deadline expires, for a card left on the "
           "reader); a good tag survives later empty snapshots; the verdict is "
           "filed under the tag scanned for THIS patient; a model refusal "
           "(esi 0) goes out as 'no score', never as BLACK; rr/age/gender/esi "
           "reach the STM32 BEFORE the verdict latches, age in years not the "
           "band index, and an unanswered screen sends 0 rather than a plausible "
           "default; demo mode ends with the session and its confidence never "
           "reaches the wire; an abort closes the wave capture exactly once; the "
           "Sensor mask expires with the link at 45 s; a PPG pulse rate never "
           "displaces an ECG one less than 10 s old\n");
    return 0;
}
