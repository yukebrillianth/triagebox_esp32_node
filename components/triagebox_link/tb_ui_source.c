/*
 * Device-side ui_mock.h: RS485 frames in, ui/logic/ reads it unchanged.
 * See tb_ui_source.h for why this file exists instead of an #ifdef.
 */
#include <string.h>

#include "esp_log.h"
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

static const char *TAG = "tb_ui_src";

/* RX task writes these, the LVGL task reads them. Copies are a few words, so a
 * spinlock beats a mutex here — and it is legal from both contexts. */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static vitals_t s_vitals;
static bool s_have_vitals;

static rfid_t s_rfid;
static bool s_rfid_ready;

/*
 * True while we are waiting for the STM32 to confirm it dropped the last
 * patient's tag. See ui_mock_start_scan() -- this is the whole fix for "press
 * Restart, start a scan, and it already has a value with no card present".
 */
static bool s_rfid_gate;

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

/* Inference result, computed once per measurement. */
static bool s_have_priority;
static ui_priority_t s_priority;
static float s_confidence;
/* The model's raw 1..5 behind s_priority, or 0 when it refused. Kept because the
 * colour cannot be un-collapsed: 3, 4 and 5 are all GREEN. */
static int s_esi;

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
        }
    }
}

void ui_mock_start_scan(void)
{
    portENTER_CRITICAL(&s_mux);
    s_rfid_ready = false;
    memset(&s_rfid, 0, sizeof(s_rfid));
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
    /* No snapshot has arrived yet, so nothing is measured -- clear both the
     * display mask and the SVM gate rather than trusting a zeroed struct. */
    if (!s_have_vitals) {
        out->valid_mask = 0U;
        out->valid = false;
    }
    portEXIT_CRITICAL(&s_mux);

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

    portENTER_CRITICAL(&s_mux);
    memcpy(tag, s_rfid.tag, sizeof(tag));
    portEXIT_CRITICAL(&s_mux);
    tag[RFID_TAG_CAPACITY - 1U] = '\0';

    /* Sent in demo mode too, deliberately: the station and the dashboard are
     * part of what is being filmed, so they have to see the same patient. */
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
    out->lora_rssi_dbm = s_rssi_dbm;
    out->lora_rssi_valid = s_rssi_valid;
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
     * Two steps, in this order.
     *
     * 1. Tell the STM32. It has to stop acquisition, park its sensors and stop
     *    transmitting before the rail drops. It does not own the rail, but it
     *    does own everything hanging off it -- and on this build the STM32 is
     *    powered FROM the ESP32 board's 3V3, so when the rail goes it goes too.
     *    Fire-and-forget: a missing or unresponsive STM32 must not block the
     *    operator's shutdown.
     * 2. Cut the rail ourselves. On board V3.0 the SW6106 PMIC owns power (no
     *    SYS_EN exists; the earlier "the STM32 owns the power rail (EXIO5/
     *    SYS_EN)" comment here was V4.0 reasoning -- on V3 EXIO5 is BLC), and it
     *    sits on the shared I2C bus at 0x3c. See ui_board_power_off().
     *
     * The delay is the STM32's head start -- the only window it gets, since it
     * loses power with us. 150 ms is enough for the register write to land and
     * for its superloop to notice, without making the button feel broken.
     *
     * Log unconditionally: silence would look like a dead button.
     */
    esp_err_t err = tb_link_send_cmd(TB_CMD_POWER_OFF);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "POWER_OFF sent to STM32 (frames_ok=%u)",
                 (unsigned)tb_link_frames_ok());
    } else {
        ESP_LOGW(TAG, "POWER_OFF not sent: %s -- cutting power anyway",
                 esp_err_to_name(err));
    }

    vTaskDelay(pdMS_TO_TICKS(150));
    ui_board_power_off();
}
