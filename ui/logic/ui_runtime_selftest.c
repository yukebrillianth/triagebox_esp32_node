#include "ui_runtime.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui_action.h"
#include "ui_mock.h"
#include "ui_nav.h"
#include "ui_session.h"

static const char *const screen_names[UI_SCREEN_COUNT] = {
    "HOME", "SCANNING", "BERHASIL", "AGE",
    "GENDER", "MENGUKUR", "RESULT", "MONITOR",
};

static void expect(ui_screen_id_t expected)
{
    assert(ui_nav_current() == expected);
    printf("%s\n", screen_names[expected]);
}

int main(void)
{
    ui_runtime_init();
    expect(UI_SCREEN_HOME);

    /* Home → Scanning (auto-starts mock scan on transition). */
    ui_action(UI_SCREEN_HOME, 1U);
    expect(UI_SCREEN_SCANNING);
    ui_runtime_tick(0);
    expect(UI_SCREEN_SCANNING);

    /* Scan completes at 500 ms → BERHASIL with RFID in session. */
    ui_runtime_tick(500);
    expect(UI_SCREEN_BERHASIL);
    assert(ui_session_has_rfid());
    assert(strcmp(ui_session_get_rfid()->tag, "3021") == 0);
    printf("rfid=%s\n", ui_session_get_rfid()->tag);

    /* Berhasil → Age → Gender → Mengukur via action tables. */
    ui_action(UI_SCREEN_BERHASIL, 0U);
    expect(UI_SCREEN_AGE);
    ui_action(UI_SCREEN_AGE, 3U);
    expect(UI_SCREEN_GENDER);
    ui_action(UI_SCREEN_GENDER, 3U);
    expect(UI_SCREEN_MENGUKUR);

    /* Entering Mengukur: measure starts once, progress 0. */
    ui_runtime_tick(500);
    assert(ui_session_get_measurement_progress() == 0);
    assert(ui_session_get_vitals()->valid);

    /* Progress is based on the 500 ms Mengukur entry tick. */
    ui_runtime_tick(1000);
    {
        uint8_t p = ui_session_get_measurement_progress();
        printf("progress_1000=%u\n", (unsigned)p);
        assert(p == 25U);
    }
    expect(UI_SCREEN_MENGUKUR);

    ui_runtime_tick(2000);
    assert(ui_session_get_measurement_progress() == 75U);
    expect(UI_SCREEN_MENGUKUR);

    /* Measure window (UI_MEASURE_MS=2000) ends at 2500 → RESULT + priority. */
    ui_runtime_tick(2500);
    expect(UI_SCREEN_RESULT);
    assert(ui_session_get_measurement_progress() == 100);
    assert(ui_session_has_priority());
    {
        ui_priority_t p = ui_session_get_priority();
        printf("priority=%d conf=%.2f reasons=\"%s\"\n",
               (int)p, (double)ui_session_get_confidence(),
               ui_session_get_reasons());
        assert(p == UI_PRIORITY_GREEN); /* cycle starts at GREEN */
    }

    /* Debug cycle key: session follows mock cycle (GREEN→YELLOW). */
    ui_runtime_debug_cycle_priority();
    assert(ui_session_get_priority() == UI_PRIORITY_YELLOW);
    printf("cycle=%d\n", (int)ui_session_get_priority());

    /* Monitor: vitals keep refreshing (live jitter). */
    ui_action(UI_SCREEN_RESULT, 0U);
    expect(UI_SCREEN_MONITOR);
    ui_runtime_tick(3000);
    assert(ui_session_get_vitals()->valid);
    assert(ui_session_has_priority()); /* priority survives Monitor */
    printf("monitor hr=%u spo2=%u\n",
           (unsigned)ui_session_get_vitals()->hr,
           (unsigned)ui_session_get_vitals()->spo2);

    /* Monitor Back → Result keeps priority; Result Reset → Home clears. */
    ui_action(UI_SCREEN_MONITOR, 0U);
    expect(UI_SCREEN_RESULT);
    assert(ui_session_has_priority());
    ui_action(UI_SCREEN_RESULT, 1U);
    expect(UI_SCREEN_HOME);
    ui_runtime_tick(3500);
    assert(!ui_session_has_priority());
    assert(!ui_session_has_rfid());

    printf("ALL_PASS measure_ms=%u\n", (unsigned)UI_MEASURE_MS);
    return 0;
}
