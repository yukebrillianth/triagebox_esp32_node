#include "ui_runtime.h"

#include "ui_mock.h"
#include "ui_nav.h"
#include "ui_session.h"

static ui_screen_id_t s_prev_screen;
static bool s_measure_started;
static uint32_t s_retriage_ms = UI_RETRIAGE_DEFAULT_MS;
static uint32_t s_retriage_due_ms;
static bool s_degraded;

static void pull_mock_vitals(void)
{
    vitals_t vitals;
    ui_mock_get_vitals(&vitals);
    ui_session_set_vitals(&vitals);
}

static void pull_mock_priority_once(void)
{
    if (!ui_session_has_priority()) {
        ui_session_set_priority(ui_mock_get_priority(),
                                ui_mock_get_confidence(),
                                ui_mock_get_reasons(),
                                ui_mock_get_esi());
    }
}

/*
 * Re-triage while Monitor is showing.
 *
 * The first result is a snapshot of one minute of a patient who may then get
 * worse -- and the box sitting on their chest is the only thing watching. So the
 * classification is re-run on a timer, and a MORE severe verdict is not left for
 * somebody to notice: it navigates to Result, which is where the colour, the
 * alarm pattern and the blink live.
 *
 * An improvement updates the session silently. It is still the current verdict,
 * so Result must show it, but nothing about a patient getting better needs to
 * grab an operator mid-triage of someone else.
 *
 * Guarded on the snapshot being complete: tb_triage_classify() answers BLACK with
 * confidence 0 when it refuses to score, so re-triaging through a sensor dropout
 * would ring the deterioration alarm for a fallen-off finger clip. HITAM has to
 * mean the model looked at a patient, not that it declined to.
 */
static void monitor_retriage_tick(uint32_t now_ms)
{
    ui_priority_t before;
    ui_priority_t after;

    if (s_retriage_ms == 0U || (now_ms - s_retriage_due_ms) < s_retriage_ms) {
        return;
    }
    s_retriage_due_ms = now_ms;

    /* No first verdict to compare against, or nothing measurable to compare it
     * with: leave both alone rather than inventing a transition. */
    if (!ui_session_has_priority() || !ui_session_get_vitals()->valid) {
        return;
    }

    before = ui_session_get_priority();
    ui_mock_reclassify();
    after = ui_mock_get_priority();
    ui_session_set_priority(after, ui_mock_get_confidence(),
                            ui_mock_get_reasons(), ui_mock_get_esi());

    if (ui_priority_degraded(before, after)) {
        s_degraded = true;
        ui_nav_go(UI_SCREEN_RESULT);
        s_prev_screen = ui_nav_current();
    }
}

void ui_runtime_set_retriage_ms(uint32_t ms)
{
    s_retriage_ms = ms;
}

uint32_t ui_runtime_retriage_ms(void)
{
    return s_retriage_ms;
}

bool ui_runtime_take_degraded(void)
{
    bool req = s_degraded;

    s_degraded = false;
    return req;
}

void ui_runtime_init(void)
{
    ui_mock_init();
    ui_nav_go(UI_SCREEN_HOME);
    s_prev_screen = ui_nav_current();
    s_measure_started = false;
    s_retriage_due_ms = 0;
    s_degraded = false;
}

void ui_runtime_tick(uint32_t now_ms)
{
    ui_screen_id_t current;

    ui_mock_tick(now_ms);

    current = ui_nav_current();
    if (current != s_prev_screen) {
        if (current == UI_SCREEN_SCANNING) {
            ui_mock_start_scan();
        }
        if (current != UI_SCREEN_MENGUKUR) {
            s_measure_started = false;
        }
        if (current == UI_SCREEN_MONITOR) {
            /* First re-triage is one full period after ENTERING Monitor, not on
             * the first tick -- entering happened because a measurement just ended,
             * so re-scoring 50 ms later would be scoring the same breaths twice. */
            s_retriage_due_ms = now_ms;
        }
        s_prev_screen = current;
    }

    switch (current) {
    case UI_SCREEN_SCANNING: {
        rfid_t rfid;
        if (ui_mock_rfid_ready(&rfid)) {
            ui_nav_on_rfid_ready(&rfid);
            s_prev_screen = ui_nav_current(); /* BERHASIL */
        }
        break;
    }
    case UI_SCREEN_MENGUKUR:
        if (!s_measure_started) {
            ui_mock_start_measure();
            s_measure_started = true;
        }
        ui_session_set_measurement_progress(ui_mock_measure_progress());
        pull_mock_vitals();
        if (ui_mock_measure_done()) {
            pull_mock_priority_once();
            ui_nav_on_measure_done();
            s_measure_started = false;
            s_prev_screen = ui_nav_current(); /* RESULT */
        }
        break;
    case UI_SCREEN_MONITOR:
        pull_mock_vitals();
        monitor_retriage_tick(now_ms);
        break;
    case UI_SCREEN_RESULT:
        pull_mock_priority_once();
        break;
    default:
        break;
    }
}

void ui_runtime_debug_cycle_priority(void)
{
    ui_priority_t before = ui_session_get_priority();
    ui_priority_t after;

    ui_mock_cycle_priority();
    if (ui_session_has_priority()) {
        after = ui_mock_get_priority();
        ui_session_set_priority(after,
                                ui_mock_get_confidence(),
                                ui_mock_get_reasons(),
                                ui_mock_get_esi());
        if (ui_nav_current() == UI_SCREEN_MONITOR && ui_priority_degraded(before, after)) {
            s_degraded = true;
            ui_nav_go(UI_SCREEN_RESULT);
            s_prev_screen = ui_nav_current();
        }
    }
}
