/*
 * Device-side ui_mock.h: RS485 frames in, ui/logic/ reads it unchanged.
 * See tb_ui_source.h for why this file exists instead of an #ifdef.
 */
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h" /* the measure-to-verdict timing log in infer_once() */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tb_link_i2c.h"
#include "tb_regs.h" /* tb_rssi_valid() */
#include "tb_triage.h"
#include "ui_demo.h"
#include "ui_session.h"
#include "tb_ui_source.h"
#include "ui_board.h"
#include "ui_mock.h"
#include "bp_capture.h"

static const char *TAG = "tb_ui_src";

/* RX task writes these, the LVGL task reads them. Copies are a few words, so a
 * spinlock beats a mutex here — and it is legal from both contexts. */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static vitals_t s_vitals;
static bool s_have_vitals;

/*
 * The BP overlay: this board's own ML result, written by the BP task
 * (bp_capture.c) and folded into every snapshot tb_ui_source_on_vital()
 * publishes. The STM32's vitals block carries bp_sys/bp_dia too -- it echoes
 * what we wrote it -- but the round trip is slow and races the display; the
 * overlay is the source of truth for the UI, and re-applying it after every
 * incoming snapshot means a poll can never momentarily blank the tile.
 *
 * s_bp_seen is the "this measurement's BP is settled" flag infer_once() waits
 * on: set by the first on_bp() after tb_ui_source_bp_arm() at measure-start,
 * valid or not, so a failed inference can't hang the triage.
 */
static bool s_bp_valid;
static uint16_t s_bp_sys;
static uint16_t s_bp_dia;
static bool s_bp_seen;

/* Caller holds s_mux. */
static void bp_apply_overlay_locked(void)
{
    if (s_bp_valid) {
        s_vitals.bp_sys = s_bp_sys;
        s_vitals.bp_dia = s_bp_dia;
        s_vitals.valid_mask |= UI_VITAL_BP;
    } else {
        s_vitals.valid_mask &= (uint8_t) ~UI_VITAL_BP;
    }
}

static rfid_t s_rfid;
static bool s_rfid_ready;

/*
 * THE TAG THIS PATIENT'S VERDICT IS FILED UNDER, latched when the scan completes
 * and not touched again until the session ends.
 *
 * s_rfid above is the live reading and every poll overwrites it once the gate is
 * open, so a second card brushed against the reader mid-measurement used to
 * change whose ID the result was sent under -- the exact failure the RFID gate
 * exists to prevent, arriving through the back door. This is written from the
 * same consume that feeds ui_session, so what the station sees and what the
 * screen shows are one identity.
 */
static rfid_t s_rfid_session;

/*
 * True while we are waiting for the STM32 to confirm it dropped the last
 * patient's tag. See ui_mock_start_scan() -- this is the whole fix for "press
 * Restart, start a scan, and it already has a value with no card present".
 */
static bool s_rfid_gate;
/*
 * When the gate was armed, and the deadline that opens it anyway.
 *
 * THE GATE CAN DEADLOCK WITHOUT THIS, and that is the "kadang RFID ga detect"
 * report. It opens on an rfid_len == 0 snapshot, but the STM32's superloop is
 * ordered take_cmd -> ServiceRfid -> publish: if the card is ALREADY on the
 * reader when START_SCAN is serviced, ForgetPatient() clears the tag and
 * ServiceRfid() re-finds it in the SAME pass, so the empty snapshot the gate
 * waits for is never published and every later tag is rejected forever.
 *
 * A deadline is sound because the question the gate asks is "was this tag found
 * before or after the STM32 processed our START_SCAN". One superloop pass
 * answers it: worst case is an RFID scan (~120 ms) plus a LoRa reply (150 ms
 * deadline + 90 ms airtime) plus a DSP pass -- under 500 ms. 1000 ms is 2x that,
 * and after it any tag in the snapshot was necessarily found post-clear, even if
 * it is physically the same card.
 */
#define RFID_GATE_MAX_MS 1000U
static uint32_t s_rfid_gate_ms;

/*
 * Button events, ring buffer. The I2C link publishes a STATE bitmask, so one
 * poll can legitimately yield up to 4 edges at once (two fingers, or a poll
 * missed while the LVGL task was busy). A single slot silently dropped all but
 * the last, which on the Age/Gender screens shows up as a key that "sometimes
 * does nothing" -- the worst kind of bug to chase.
 *
 * 8 is two full press+release cycles on all four keys: more than a human can
 * generate inside one 50 ms tick, and 96 bytes.
 */
#define BTN_QUEUE_LEN 8U
static btn_event_t s_btn_q[BTN_QUEUE_LEN];
static uint8_t s_btn_head; /* next write */
static uint8_t s_btn_tail; /* next read */
static uint32_t s_btn_dropped;

static uint8_t s_sensor_mask;
static bool s_lora_ok;
static bool s_lora_reported;
static int8_t s_rssi_dbm;
static bool s_rssi_valid;
static uint32_t s_last_frame_ms;
static bool s_frame_seen;

/* Measure window state, owned by the LVGL task only — no lock needed. */
static uint32_t s_now_ms;
static bool s_measure_active;
static bool s_measure_done;
static uint32_t s_measure_start_ms;
/* Same instant as s_measure_start_ms, on a clock the LVGL task cannot stall --
 * see the timing log in infer_once(). */
static int64_t s_measure_start_us;
static bool s_timing_logged;

/* Inference result, computed once per measurement. */
static bool s_have_priority;
static ui_priority_t s_priority;
static float s_confidence;
/* The model's raw 1..5 behind s_priority, or 0 when it refused. Kept because the
 * colour cannot be un-collapsed: 3, 4 and 5 are all GREEN. */
static int s_esi;

/*
 * ECG FIRST: hold the last limb-lead rate through a PPG-only patch.
 *
 * The STM32 publishes ONE rate (TB_REG_HR) and a bit saying which sensor won --
 * TB_FLAG_HR_FROM_PPG means it is a pulse rate counted at the finger, because
 * the ECG produced nothing that poll. The ECG on this build is a limb lead:
 * clamps on both wrists and one on a leg (Einthoven), not chest electrodes.
 *
 * On this board revision the MAX30102's rate flickers -- the finger clip loses
 * contact on the slightest movement -- so a display fed straight from the wire
 * jumps between an ECG heart rate and a PPG pulse rate several times a minute
 * and the operator cannot tell which is which.
 *
 * So a PPG-sourced reading does not overwrite a recent ECG one. HR_ECG_HOLD_MS
 * bounds it: past that the ECG number is history, and a live pulse rate beats a
 * stale heart rate.
 *
 * IT FALLS BACK TO PPG rather than blanking, and that is deliberate: HR reaches
 * tb_classify() as a feature, which refuses to score at all on hr <= 0 and
 * returns BLACK/esi 0 -- the same refusal the manual RR screen exists to avoid.
 * A rate labelled two beats optimistically is worth far more than a verdict the
 * model declined to give. `hr_from_ppg` stays honest in the snapshot either way,
 * and `i2clink` reads the raw flag straight off the wire.
 *
 * Delete this the day the sensor PCB is reprinted with a finger clip that holds.
 */
#define HR_ECG_HOLD_MS 10000U
static uint16_t s_hr_ecg;
static uint32_t s_hr_ecg_ms;
static bool s_hr_ecg_seen;
/*
 * Did the ECG produce a rate at any point in THIS measurement window? Cleared by
 * tb_ui_source_bp_arm() at measure-start, so it answers a question about one
 * patient rather than about the session.
 *
 * The BP gate reads it (see bp_capture.c). Separate from s_hr_ecg_seen above,
 * which deliberately survives across measurements because it backs a display
 * hold, not a publish decision.
 */
static bool s_ecg_rate_in_window;

/* Caller holds s_mux. Rewrites v in place, so every consumer -- tiles, model,
 * the Result copy -- reads one number and one provenance. */
static void hr_prefer_ecg_locked(vitals_t *v)
{
    if ((v->valid_mask & UI_VITAL_HR) == 0U) {
        return;
    }
    if (!v->hr_from_ppg) {
        s_hr_ecg = v->hr;
        s_hr_ecg_ms = s_now_ms;
        s_hr_ecg_seen = true;
        s_ecg_rate_in_window = true;
        return;
    }
    if (s_hr_ecg_seen && ((s_now_ms - s_hr_ecg_ms) < HR_ECG_HOLD_MS)) {
        v->hr = s_hr_ecg;
        /* Not a lie by omission: the number in v->hr now IS the ECG's, so the
         * provenance bit describes it correctly. */
        v->hr_from_ppg = false;
    }
}

bool tb_ui_source_ecg_rate_seen(void)
{
    portENTER_CRITICAL(&s_mux);
    const bool seen = s_ecg_rate_in_window;
    portEXIT_CRITICAL(&s_mux);
    return seen;
}

/* ---------------------------------------------------------------- RX task -- */

void tb_ui_source_on_vital(const vitals_t *v)
{
    if (v == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    s_vitals = *v;
    s_have_vitals = true;
    hr_prefer_ecg_locked(&s_vitals);
    bp_apply_overlay_locked();
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_on_bp(bool valid, uint16_t sys, uint16_t dia)
{
    /* Outside the lock: a failed write logs, it does not spin. Sending only a
     * valid pair is what keeps TB_FLAG_BP_VALID honest on the STM32. */
    if (valid) {
        (void)tb_link_send_bp(sys, dia);
    }

    portENTER_CRITICAL(&s_mux);
    s_bp_valid = valid;
    s_bp_sys = sys;
    s_bp_dia = dia;
    s_bp_seen = true; /* settled, whatever the verdict: infer_once() unblocks */
    /* Publish into the current snapshot too: between the BP task's publish
     * and the next 50 ms poll, Result would otherwise keep showing the tile
     * as "--" for up to one poll. apply_vital_tiles reads the snapshot. */
    if (s_have_vitals) {
        bp_apply_overlay_locked();
    }
    portEXIT_CRITICAL(&s_mux);
}

bool tb_ui_source_bp_ready(void)
{
    portENTER_CRITICAL(&s_mux);
    const bool ready = s_bp_seen;
    portEXIT_CRITICAL(&s_mux);
    return ready;
}

void tb_ui_source_bp_arm(void)
{
    portENTER_CRITICAL(&s_mux);
    s_bp_seen = false;
    s_bp_valid = false;
    s_bp_sys = 0U;
    s_bp_dia = 0U;
    s_ecg_rate_in_window = false;
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_on_button(uint8_t index, bool pressed)
{
    uint8_t next;

    if (index > 3U) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    next = (uint8_t)((s_btn_head + 1U) % BTN_QUEUE_LEN);
    if (next == s_btn_tail) {
        /* Full. Drop the NEWEST rather than overwriting the oldest: the UI is
         * mid-sequence on the events already queued, and reordering them would
         * be worse than losing one. Counted so it is not silent. */
        ++s_btn_dropped;
    } else {
        s_btn_q[s_btn_head].index = index;
        s_btn_q[s_btn_head].pressed = pressed;
        s_btn_q[s_btn_head].timestamp_ms = s_now_ms;
        s_btn_head = next;
    }
    portEXIT_CRITICAL(&s_mux);
}

uint32_t tb_ui_source_buttons_dropped(void)
{
    return s_btn_dropped;
}

void tb_ui_source_on_rfid(const rfid_t *r)
{
    if (r == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    if (s_rfid_gate) {
        /* An empty snapshot is the STM32 saying "old tag gone"; only then do we
         * start believing tags again. Anything with a tag still in it was latched
         * before our START_SCAN landed, whatever its contents. */
        if (!r->present) {
            s_rfid_gate = false;
        } else if ((s_now_ms - s_rfid_gate_ms) >= RFID_GATE_MAX_MS) {
            /* The empty snapshot is never coming: the card was already on the
             * reader, so the STM32 cleared and re-found it inside one pass. Past
             * the deadline the tag is necessarily post-clear -- see
             * RFID_GATE_MAX_MS. Accept it here rather than waiting forever,
             * which is what "kadang RFID ga detect" was. */
            s_rfid_gate = false;
            s_rfid = *r;
            s_rfid_ready = true;
        }
    } else if (r->present) {
        s_rfid = *r;
        s_rfid_ready = true;
    }
    /* An empty snapshot with the gate open is deliberately ignored rather than
     * clearing s_rfid: the STM32 keeps publishing the tag it found, but if it
     * ever stops, Monitor and Result must not lose the ID they are showing.
     * ui_mock_start_scan() is the one place a tag is forgotten. */
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_on_status(uint8_t sensor_ok_mask, uint8_t battery, int lora_ok)
{
    portENTER_CRITICAL(&s_mux);
    s_sensor_mask = sensor_ok_mask;
    /* Battery also rides on VITAL, but STATUS keeps arriving while idle. */
    s_vitals.battery = battery;
    if (lora_ok >= 0) {
        s_lora_ok = (lora_ok != 0);
        s_lora_reported = true;
    }
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_on_rssi(int8_t dbm)
{
    /*
     * An out-of-range byte does NOT clear a previously good reading, it is simply
     * not adopted. That distinction matters while range-testing: the STM32 zeroes
     * this field until the next poll arrives, and polls are 15 s apart, so
     * treating "not yet" as "no signal" would blank the number for most of every
     * cycle and make it unreadable exactly when someone is walking with it.
     */
    if (!tb_rssi_valid(dbm)) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    s_rssi_dbm = dbm;
    s_rssi_valid = true;
    portEXIT_CRITICAL(&s_mux);
}

void tb_ui_source_mark_frame(void)
{
    portENTER_CRITICAL(&s_mux);
    s_last_frame_ms = s_now_ms;
    s_frame_seen = true;
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
    memset(&s_rfid_session, 0, sizeof(s_rfid_session));
    s_rfid_gate = false; /* Nothing to clear yet; the first scan need not wait. */
    memset(s_btn_q, 0, sizeof(s_btn_q));
    s_btn_head = 0;
    s_btn_tail = 0;
    s_btn_dropped = 0;
    s_sensor_mask = 0;
    s_lora_ok = false;
    s_lora_reported = false;
    s_rssi_dbm = 0;
    s_rssi_valid = false;
    s_last_frame_ms = 0;
    s_frame_seen = false;
    s_bp_valid = false;
    s_bp_sys = 0U;
    s_bp_dia = 0U;
    s_bp_seen = false;
    s_hr_ecg = 0U;
    s_hr_ecg_ms = 0U;
    s_hr_ecg_seen = false;
    s_ecg_rate_in_window = false;
    portEXIT_CRITICAL(&s_mux);

    s_now_ms = 0;
    s_measure_active = false;
    s_measure_done = false;
    s_measure_start_ms = 0;
    s_have_priority = false;
    s_priority = UI_PRIORITY_BLACK;
    s_confidence = 0.0f;
    s_esi = 0;
}

void ui_mock_tick(uint32_t now_ms)
{
    s_now_ms = now_ms;

    if (s_measure_active && !s_measure_done) {
        if ((now_ms - s_measure_start_ms) >= UI_MEASURE_MS) {
            s_measure_done = true;
            s_measure_active = false;
            /* Freeze the waveform and wake the BP task. Same task as this
             * tick (LVGL), so no ordering hazard: bp_capture_publish-wait in
             * infer_once() comes later in this same task's timeline. The BP
             * task runs concurrently and publishes via on_bp(); infer_once
             * polls tb_ui_source_bp_ready() with a bounded wait. */
            bp_capture_measure_done();
        }
    }
}

void ui_mock_start_scan(void)
{
    portENTER_CRITICAL(&s_mux);
    s_rfid_ready = false;
    memset(&s_rfid, 0, sizeof(s_rfid));
    /* The previous patient's identity is void from here: BERHASIL's "Restart"
     * reaches this without passing Home, so end_session is not guaranteed to
     * have cleared it. A measurement can then only report a tag this scan
     * produced. */
    memset(&s_rfid_session, 0, sizeof(s_rfid_session));
    /*
     * Clearing locally is not enough. The STM32 clears its published tag when it
     * services START_SCAN, but that is up to a superloop later, so the next one or
     * two 50 ms polls still return the *previous* patient's card -- and the
     * scanning screen accepts it instantly. That is the reported "press Restart
     * and it already has a value with no card present", and it attaches one
     * patient's vitals to another's ID.
     *
     * So stop trusting tags until a snapshot arrives with rfid_len == 0, which
     * only the STM32 can produce and only after it has processed this command.
     *
     * Gated even when the write below fails: a scan that never completes is a
     * visible fault, where a scan that completes with the wrong identity is not.
     */
    s_rfid_gate = true;
    s_rfid_gate_ms = s_now_ms;
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
        /* The latch for infer_once(). Done at the consume, not at measure-start,
         * so the Age/Gender/Airway/RR screens between the two cannot let a stray
         * tag into the identity. Only the LVGL task writes or reads
         * s_rfid_session, so the lock is not what protects it. */
        s_rfid_session = s_rfid;
        s_rfid_ready = false; /* one-shot, same contract as the mock */
    }
    portEXIT_CRITICAL(&s_mux);

    return ready;
}

/*
 * Session over. The STM32 keeps the last triage verdict standing on purpose, and
 * the station publishes a vital for as long as a verdict stands -- so without this
 * the node reports the departed patient every 15 s forever, with every measurement
 * blank because nobody is on the sensors, and those blanks are the newest readings
 * the dashboard has.
 *
 * Local state is cleared too, not just the command: a failed write must not leave
 * this board showing a patient it has just told the operator is gone. The STM32
 * side is the one that can be lost, and the cost of losing it is the bug above
 * returning until the next scan -- which clears the same state on that board.
 */
void ui_mock_end_session(void)
{
    const bool measuring = s_measure_active;
    /*
     * Was there a patient at all? This is what the failed-ABORT warning below is
     * about, so it is what gates it -- and it must be sampled before the clears.
     *
     * A tag OR a verdict, because either is something the STM32 is still holding:
     * START_SCAN latched the card, or a RESULT write latched the colour.
     */
    bool had_patient = s_have_priority;

    portENTER_CRITICAL(&s_mux);
    had_patient = had_patient || s_rfid.present || s_rfid_session.present;
    s_rfid_ready = false;
    memset(&s_rfid, 0, sizeof(s_rfid));
    memset(&s_rfid_session, 0, sizeof(s_rfid_session));
    /* Left OPEN, not armed: the gate exists to reject a tag latched before a
     * START_SCAN, and there is no scan running to protect. Arming it here would
     * make the next scan wait for an empty snapshot it has no reason to need. */
    s_rfid_gate = false;
    portEXIT_CRITICAL(&s_mux);

    s_measure_active = false;
    s_measure_done = false;
    s_have_priority = false;

    /*
     * Demo mode ends with the session it was filmed in.
     *
     * It used to be a static outside any session, so a take that finished with
     * the switch left on meant the NEXT patient -- a real one -- was shown
     * MERAH/0.93/ESI 1 over fabricated vitals. That is the same hazard ui_demo.h
     * already argues for not persisting it to NVS ("the convenience is the
     * hazard"), one scope smaller. The cost is re-arming it from the Menu per
     * take, which is one switch.
     */
    ui_demo_set(false);

    /*
     * Close the waveform capture, or an abort mid-measurement leaves it running
     * for the rest of the session: s_capturing stays true, so poll_wave_if_due()
     * keeps pulling the 124-byte block -- a ~25 ms transaction every 50 ms on the
     * bus the GT911 touch panel shares -- for a patient who has left.
     *
     * ONLY when a window was actually open. This function also runs on every
     * arrival at Home, including the one ui_runtime_init() does at boot, and
     * bp_capture_measure_done() is a notify: calling it with no capture running
     * would re-run the BP task over the PREVIOUS window still sitting in its
     * accumulator and publish that pressure again, for the patient who just left.
     * A window that closed normally already closed the capture with it.
     */
    if (measuring) {
        bp_capture_measure_done();
    }

    if ((tb_link_send_cmd(TB_CMD_ABORT) != ESP_OK) && had_patient) {
        /*
         * Gated on there having been a patient, because ui_runtime_init() runs
         * before tb_link_start(): its navigation to Home reached this line at
         * every boot, with no link and nobody scanned, and a warning about a
         * patient who does not exist is how an operator learns to ignore the real
         * one. Nothing weaker would do -- the send fails on a dead link too, and
         * a dead link is exactly when a standing verdict cannot be cleared.
         */
        ESP_LOGW(TAG, "ABORT not sent -- STM32 may keep reporting this patient");
    }
}

void ui_mock_start_measure(void)
{
    s_measure_active = true;
    s_measure_done = false;
    s_measure_start_ms = s_now_ms;
    s_measure_start_us = esp_timer_get_time();
    s_timing_logged = false;
    s_have_priority = false;

    /* Fresh BP for this measurement: clear the overlay AND arm the "settled"
     * flag infer_once() waits on. Ordering with the capture start does not
     * matter (both are this task); ordering with the poll task's pushes does,
     * and bp_capture_start() is what makes bp_capture_capturing() true. */
    tb_ui_source_bp_arm();
    bp_capture_start();

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
    /* No snapshot has arrived yet, so nothing is measured -- clear both the
     * display mask and the SVM gate rather than trusting a zeroed struct. */
    if (!s_have_vitals) {
        out->valid_mask = 0U;
        out->valid = false;
    }
    portEXIT_CRITICAL(&s_mux);

    /*
     * The operator's counted respiratory rate, filled in outside the lock
     * because it is session state owned by this task, not something the STM32
     * publishes.
     *
     * Only when the wire did not bring one: the microphone does not exist today
     * so TB_FLAG_RR_VALID is never set, but if it ever ships, a measured rate
     * must win over a typed one. Without this the model saw rr = 0, and
     * tb_classify() refuses on respiratory_rate <= 0 -- so every real patient
     * scored BLACK with esi 0 no matter how good the other vitals were.
     */
    if (((out->valid_mask & UI_VITAL_RR) == 0U) && ui_session_has_rr()) {
        out->rr = ui_rr_band_value(ui_session_get_rr());
        out->valid_mask |= UI_VITAL_RR;
    }

    /* Last, and outside the lock: the real snapshot is copied first so the demo
     * patient inherits the true battery reading, and so switching demo off
     * returns to live data with nothing to undo. */
    if (ui_demo_enabled()) {
        ui_demo_vitals(s_now_ms, out);
    }
}

/* Runs the model once per measurement and reports the result to the STM32,
 * which owns the LoRa TX. ui_runtime.c calls this exactly once via
 * pull_mock_priority_once(). */
static void infer_once(void)
{
    vitals_t v;
    char tag[RFID_TAG_CAPACITY];

    if (s_have_priority) {
        return;
    }

    /*
     * Wait for this measurement's BP to settle before classifying: the BP task
     * was woken at measure-done (ui_mock_tick, this same task) and publishes
     * through on_bp() under the same spinlock the snapshot uses. Bounded at
     * 2 s -- a hung or slow inference must delay the triage verdict, not hang
     * the UI, and a missed BP is exactly what the model's 129.7 imputation is
     * for. 50 ms steps because that is the poll cadence the BP task's inputs
     * ran on; there is nothing finer to react to.
     */
    for (unsigned waited = 0U; !tb_ui_source_bp_ready() && (waited < 2000U);
         waited += 50U) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (!tb_ui_source_bp_ready()) {
        ESP_LOGW(TAG, "BP not settled in 2s -- classifying without it");
    }

    /*
     * The last snapshot, not an aggregate over the window. The model takes seven
     * instantaneous scalars -- what a triage nurse would have written down once --
     * so there is nothing for a mean or a min to fill.
     */
    ui_mock_get_vitals(&v);

    if (ui_demo_enabled()) {
        /*
         * Substituted rather than fed through the model: the demo vitals would
         * score somewhere plausible, but "somewhere plausible" is not a fixed
         * colour, and a take that comes out YELLOW is a wasted take. Logged at
         * WARN on every result because this is the one code path that reports a
         * triage nobody measured.
         */
        s_priority = UI_DEMO_PRIORITY;
        s_confidence = UI_DEMO_CONFIDENCE;
        s_esi = UI_DEMO_ESI;
        ESP_LOGW(TAG, "DEMO MODE: reporting priority=%d esi=%d, model not run",
                 (int)s_priority, s_esi);
    } else {
        int esi = 0;

        s_priority = tb_triage_classify(&v, ui_session_get_age(),
                                        ui_session_get_gender(),
                                        ui_session_get_airway(), &s_confidence,
                                        &esi);
        /* ESI logged alongside the colour because the colour cannot be
         * un-collapsed: 3, 4 and 5 are all GREEN. airway too, because it is what
         * separates a RED the model chose from a RED the operator forced. */
        ESP_LOGI(TAG, "triage: priority=%d esi=%d airway=%d confidence=%.2f "
                      "valid=%d hr=%u spo2=%u rr=%u sbp=%u",
                 (int)s_priority, esi, (int)ui_session_get_airway(),
                 (double)s_confidence, (int)v.valid,
                 (unsigned)v.hr, (unsigned)v.spo2, (unsigned)v.rr,
                 (unsigned)v.bp_sys);
        s_esi = esi;
    }
    s_have_priority = true;

    /*
     * START_MEASURE to verdict, measured rather than inferred from
     * 10 s-granularity heartbeat logs -- someone will ask for this number in a
     * demo and "about fifty seconds" is not an answer.
     *
     * esp_timer_get_time() and NOT s_now_ms: this function runs on the LVGL task
     * and its BP wait above blocks that task, so ui_mock_tick() cannot advance
     * s_now_ms while we sit here. Timing the wait with a clock the wait itself
     * freezes would report the post-window cost as 0 ms, every time.
     *
     * `post-window` is the only part a code change can move: the BP bounded wait,
     * the model run and the STM32 write, after the fixed UI_MEASURE_MS the
     * operator waits through.
     */
    {
        const int32_t total_ms =
            (int32_t)((esp_timer_get_time() - s_measure_start_us) / 1000);

        /* First verdict of this measurement only. A Monitor re-triage reaches
         * this same function minutes later, and its elapsed time says nothing
         * about how long a patient takes. */
        if (!s_timing_logged) {
            s_timing_logged = true;
            ESP_LOGI(TAG, "timing: verdict %d ms after START_MEASURE "
                          "(window %d, model+BP %d)",
                     (int)total_ms, (int)UI_MEASURE_MS,
                     (int)(total_ms - (int32_t)UI_MEASURE_MS));
        }
    }

    /*
     * The tag LATCHED WHEN THIS PATIENT WAS SCANNED, not the live s_rfid -- see
     * s_rfid_session. LVGL-task state on both ends, so no lock; the copy keeps
     * the NUL guard the wire cannot promise.
     */
    memcpy(tag, s_rfid_session.tag, sizeof(tag));
    tag[RFID_TAG_CAPACITY - 1U] = '\0';

    /*
     * The four things the STM32 cannot measure, sent BEFORE the verdict below.
     *
     * ORDERING IS LOAD-BEARING, not tidiness: the slave latches the verdict as
     * complete when TB_REG_CONFIDENCE is written, so the ESI has to be sitting in
     * its register before that byte lands. Written afterwards it would be stamped
     * into the NEXT patient's packet while this one published the PREVIOUS
     * patient's ESI -- silently, because the colour would look right on both.
     * RR/age/sex are patient facts rather than parts of the verdict and only have
     * to precede it, so one group before the result is the rule with nothing left
     * to remember.
     *
     * Sent on the refusal path too. Harmless and honest: TB_PRIORITY_WIRE_NONE
     * makes the station withhold the whole vital, so an esi 0 beside three facts
     * never reaches MQTT -- and a refusal must not become publishable just
     * because these registers now hold something.
     *
     * Demo mode sends all four exactly as a real run does, s_esi included: the
     * dashboard is part of what gets filmed, and the zeroed confidence byte below
     * stays the single unambiguous marker rather than being diluted by a second.
     * The three facts are the operator's real answers either way.
     */
    {
        /* The OPERATOR'S counted rate, which is the rate the verdict came from --
         * ui_mock_get_vitals() substitutes exactly this into the vitals scored
         * above, because the mic PCB does not exist and tb_classify() refuses on
         * rr <= 0. Without it the station published nothing while the verdict was
         * computed from 16 breaths/min. 0 = the RR screen was never answered.
         * The clamp is unreachable through ui_rr_band_value()'s four values
         * (10/16/25/36) and is here so a fifth band cannot silently wrap. */
        const uint16_t rr = ui_session_has_rr()
                            ? ui_rr_band_value(ui_session_get_rr()) : 0U;
        /* YEARS, not the band index: tb_triage_age_years() returns the band's
         * clinical mid-point (12/31/53/70), which is the number the model
         * scored, so the station reports the verdict's input rather than a code
         * only this UI knows. That switch bounds it to 12..70, so the round needs
         * no clamp of its own. 0 = the Age screen was never answered, and no band
         * can produce it. */
        const uint8_t age = ui_session_has_age()
                            ? (uint8_t)(tb_triage_age_years(ui_session_get_age())
                                        + 0.5f)
                            : 0U;
        /* ui_gender_t's enumerators ARE the ASCII codes, so this is a cast and
         * not a table. UNKNOWN and never-asked both go out as 0 rather than 'U':
         * the station omits the key instead of publishing a sex nobody entered,
         * which is the same rule the absent measurements follow. Testing for the
         * two real answers rather than ui_session_has_gender() keeps that
         * decision in one place -- ui_session's reset default is U, so an
         * unanswered screen lands on 0 either way. */
        const ui_gender_t sex = ui_session_get_gender();
        const uint8_t sex_ascii =
            ((sex == UI_GENDER_M) || (sex == UI_GENDER_F)) ? (uint8_t)sex : 0U;

        if (tb_link_send_patient((uint8_t)((rr > 255U) ? 255U : rr), age,
                                 sex_ascii,
                                 (uint8_t)((s_esi > 0) ? s_esi : 0)) != ESP_OK) {
            ESP_LOGW(TAG, "patient fields not sent -- the station will publish "
                          "this verdict without rr/age/sex/esi");
        }
    }

    if (s_esi == 0) {
        /*
         * A REFUSAL IS NOT A VERDICT. esi 0 is tb_classify() declining to score
         * because a vital is missing -- a fallen-off finger clip, an unanswered
         * RR screen -- and the BLACK it returns alongside is a placeholder, not
         * EXPECTANT. But wire 0 IS BLACK, so forwarding that colour published a
         * genuine EXPECTANT triage for a living patient. TB_PRIORITY_WIRE_NONE
         * is the station's "no score" byte instead: lora_vital_priority_name()
         * answers NULL to it, and its contract is to omit the key and withhold
         * the whole vital -- the node stays ONLINE, only the verdict it does not
         * have stops. One register byte; nothing changes on the STM32 or the
         * station.
         */
        if (tb_link_send_unscored() != ESP_OK) {
            ESP_LOGW(TAG, "UNSCORED not sent -- the STM32 may still be standing "
                          "on an older verdict");
        }
    } else {
        /*
         * Sent in demo mode too, deliberately: the station and the dashboard are
         * part of what is being filmed, so they have to see the same patient.
         *
         * What does NOT travel is demo mode's confidence. 0.93 is invented, and
         * it is the one field that would make the record read as a measurement
         * on the backend. 0.00 marks it instead, and the mark is unambiguous
         * rather than merely conventional: a scored verdict's confidence is the
         * winning class probability of a 5-class softmax, so it cannot be below
         * 0.20, and a refusal no longer reaches this branch at all. A colour with
         * confidence 0 can therefore only be demo mode.
         *
         * The screen keeps 0.93 -- that is the act -- and the permanent on-screen
         * DEMO marker is its disclosure to whoever is holding the box.
         */
        const float wire_confidence = ui_demo_enabled() ? 0.0f : s_confidence;

        if (tb_link_send_result(s_priority, wire_confidence,
                                tag[0] ? tag : NULL) != ESP_OK) {
            ESP_LOGW(TAG, "RESULT not sent — station will miss this triage");
        }
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

int ui_mock_get_esi(void)
{
    infer_once();
    return s_esi;
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

void ui_mock_reclassify(void)
{
    /*
     * Drop the latch infer_once() holds, so the next ui_mock_get_priority() scores
     * the CURRENT snapshot. Called by the monitor re-triage timer.
     *
     * That re-send to the STM32 is deliberate, not a side effect: a patient who
     * deteriorates has to reach the station and the dashboard as the new colour,
     * and tb_link_send_result() inside infer_once() is the only path there.
     */
    s_have_priority = false;
}

void ui_mock_push_button(uint8_t index, bool pressed)
{
    tb_ui_source_on_button(index, pressed);
}

bool ui_mock_pop_button(btn_event_t *out)
{
    bool pending;

    portENTER_CRITICAL(&s_mux);
    pending = (s_btn_head != s_btn_tail);
    if (pending) {
        if (out != NULL) {
            *out = s_btn_q[s_btn_tail];
        }
        s_btn_tail = (uint8_t)((s_btn_tail + 1U) % BTN_QUEUE_LEN);
    }
    portEXIT_CRITICAL(&s_mux);

    return pending;
}

/*
 * The Sensor dot ages with the link, on the same rule as the System dot's
 * LINK_STALE_MS in ui_status.c: 45 s is one missed STATUS at the 15/30 s wire
 * cadences, so past it the STM32 is gone rather than idle.
 *
 * A second definition and not a shared one because ui_status.h exports the
 * decision (ui_status_system) and not the constant, and this file must not
 * include ui_status.h -- the desktop sim links that module without this one. The
 * two are in different translation units, so nothing catches a drift: move both
 * or neither.
 */
#define LINK_STALE_MS 45000U

void ui_mock_get_link_status(link_status_t *out)
{
    if (out == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    out->sensor_mask = s_sensor_mask;
    out->lora_ok = s_lora_ok;
    out->lora_reported = s_lora_reported;
    out->link_age_ms = s_frame_seen ? (s_now_ms - s_last_frame_ms) : 0U;
    out->link_never_seen = !s_frame_seen;
    /*
     * A mask from a board that has stopped talking describes its last breath,
     * not its health -- hand the dot the mask only while the link that produced
     * it is alive. Zeroed rather than aged in place because
     * ui_status_sensors() takes no age parameter; mask 0 is its ERROR answer,
     * which is the correct colour for a sensor board that went quiet.
     */
    if (out->link_never_seen || (out->link_age_ms > LINK_STALE_MS)) {
        out->sensor_mask = 0U;
    }
    out->lora_rssi_dbm = s_rssi_dbm;
    out->lora_rssi_valid = s_rssi_valid;
    out->polls_ok = tb_link_frames_ok();
    out->polls_failed = tb_link_crc_errors();
    portEXIT_CRITICAL(&s_mux);

    /* Only the sensor dot is faked. The Sistem and LoRa dots stay honest because
     * they report the STM32 link, which demo mode does not replace -- the RFID
     * tag still comes over it, so a dead link is still a dead demo. RSSI is left
     * alone for the same reason and one more: the point of putting it on screen
     * is measuring real range, so a fake number there would defeat the feature. */
    if (ui_demo_enabled()) {
        out->sensor_mask = ui_demo_sensor_mask();
    }
}

void ui_mock_power_off(void)
{
    /*
     * Cut the rail FIRST. On success ui_board_power_off() does not return -- see
     * it -- so this is the whole function on the normal path.
     *
     * The old order was command, 150 ms head start, then cut, on the reasoning
     * that the STM32 should park its sensors and stop transmitting before losing
     * power. Two things make it the wrong order. It arms the failure that
     * matters: ui_board_power_off() returns from seven paths with no way to check
     * (a busy bus, a NAKing PMIC, key-off disabled in OTP), and every one of them
     * used to leave a box that is still running, still on the sensors, and has
     * already told its sensor board to go quiet. And the head start buys nothing
     * today -- the STM32's command dispatch acts on START_SCAN and ABORT only,
     * and records POWER_OFF without parking anything (main.c says so). It loses
     * power with us either way: it is fed from this board's 3V3, and on V3.0 the
     * SW6106 owns that rail.
     */
    ui_board_power_off();

    /*
     * Only reachable when the power-off did not happen. Send the command anyway,
     * so the day the STM32 does act on it a box that refused to die still stops
     * reporting -- and log at WARN, because a power button that did nothing must
     * not be silent.
     */
    ESP_LOGW(TAG, "still running after power-off; telling the STM32 anyway");
    (void)tb_link_send_cmd(TB_CMD_POWER_OFF);
}
