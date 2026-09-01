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
    "GENDER", "AIRWAY", "MENGUKUR", "RESULT", "MONITOR", "TEST",
};

static void expect(ui_screen_id_t expected)
{
    assert(ui_nav_current() == expected);
    printf("%s\n", screen_names[expected]);
}

/*
 * ui_priority_degraded() is the whole point of the re-triage feature, so its
 * ordering is pinned directly: GREEN < YELLOW < RED < BLACK, in severity, NOT in
 * the enum's own numbering (RED=0). A cast-based "to < from" would get every one
 * of these backwards.
 */
static void test_severity_order(void)
{
    assert(ui_priority_degraded(UI_PRIORITY_GREEN, UI_PRIORITY_YELLOW));
    assert(ui_priority_degraded(UI_PRIORITY_YELLOW, UI_PRIORITY_RED));
    assert(ui_priority_degraded(UI_PRIORITY_RED, UI_PRIORITY_BLACK));
    assert(ui_priority_degraded(UI_PRIORITY_GREEN, UI_PRIORITY_BLACK));

    /* Improvement and no-change are not degradations. */
    assert(!ui_priority_degraded(UI_PRIORITY_RED, UI_PRIORITY_GREEN));
    assert(!ui_priority_degraded(UI_PRIORITY_BLACK, UI_PRIORITY_RED));
    assert(!ui_priority_degraded(UI_PRIORITY_RED, UI_PRIORITY_RED));

    /* The trap the function exists to avoid: RED (enum 0) is MORE severe than
     * YELLOW (enum 1), so a numeric compare would call YELLOW->RED an
     * improvement and stay silent on the one transition that matters most. */
    assert((int)UI_PRIORITY_RED < (int)UI_PRIORITY_YELLOW);
    printf("severity_order OK\n");
}

/*
 * Monitor re-triage: on a tick past the interval, a MORE severe verdict jumps to
 * Result and raises the degraded flag; an equal one is silent. Driven through the
 * mock's priority cycle, which is what a desktop can steer.
 */
static void test_monitor_retriage(void)
{
    /* Walk a fresh patient to Monitor. */
    ui_runtime_init();
    ui_runtime_set_retriage_ms(15000U);
    ui_action(UI_SCREEN_HOME, 1U);
    ui_runtime_tick(500);           /* scan completes -> BERHASIL */
    ui_action(UI_SCREEN_BERHASIL, 0U);
    ui_action(UI_SCREEN_AGE, 3U);
    ui_action(UI_SCREEN_GENDER, 3U);
    ui_action(UI_SCREEN_AIRWAY, 3U);
    ui_runtime_tick(1000);          /* measure starts */
    ui_runtime_tick(3500);          /* window (2000) ends -> RESULT, GREEN */
    expect(UI_SCREEN_RESULT);
    assert(ui_session_get_priority() == UI_PRIORITY_GREEN);
    ui_action(UI_SCREEN_RESULT, 0U);
    expect(UI_SCREEN_MONITOR);

    /* Before the interval elapses: nothing fires even if the colour would move. */
    ui_mock_cycle_priority();       /* mock now offers YELLOW */
    ui_runtime_tick(4000);          /* only 500 ms into the 15 s interval */
    expect(UI_SCREEN_MONITOR);
    assert(!ui_runtime_take_degraded());

    /* Past the interval, with a worse colour on offer: jump + flag. The session
     * entered Monitor as GREEN; the mock now classifies YELLOW. */
    ui_runtime_tick(20000);
    expect(UI_SCREEN_RESULT);
    assert(ui_session_get_priority() == UI_PRIORITY_YELLOW);
    assert(ui_runtime_take_degraded());
    assert(!ui_runtime_take_degraded()); /* take-once */

    /* Off (interval 0): no re-triage even long past any interval. */
    ui_runtime_set_retriage_ms(0U);
    ui_action(UI_SCREEN_RESULT, 0U);
    expect(UI_SCREEN_MONITOR);
    ui_mock_cycle_priority();       /* would be RED next */
    ui_runtime_tick(60000);
    expect(UI_SCREEN_MONITOR);
    assert(!ui_runtime_take_degraded());

    printf("monitor_retriage OK\n");
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
    /* Third manual input. Select commits the airway answer and only then does
     * measuring start -- the model treats a set airway flag as RED on its own,
     * so it has to be asked before the reading, not after. */
    expect(UI_SCREEN_AIRWAY);
    assert(!ui_session_has_airway());
    ui_action(UI_SCREEN_AIRWAY, 3U);
    expect(UI_SCREEN_MENGUKUR);
    assert(ui_session_has_airway());
    assert(!ui_session_get_airway()); /* "Tidak ada" is the pre-highlighted row */

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

    test_severity_order();
    test_monitor_retriage();

    printf("ALL_PASS measure_ms=%u\n", (unsigned)UI_MEASURE_MS);
    return 0;
}
