#include "ui_runtime.h"

#include "ui_mock.h"
#include "ui_nav.h"
#include "ui_session.h"

static ui_screen_id_t s_prev_screen;
static bool s_measure_started;

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
                                ui_mock_get_reasons());
    }
}

void ui_runtime_init(void)
{
    ui_mock_init();
    ui_nav_go(UI_SCREEN_HOME);
    s_prev_screen = ui_nav_current();
    s_measure_started = false;
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
    ui_mock_cycle_priority();
    if (ui_session_has_priority()) {
        ui_session_set_priority(ui_mock_get_priority(),
                                ui_mock_get_confidence(),
                                ui_mock_get_reasons());
    }
}
