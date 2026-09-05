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
    "GENDER", "AIRWAY", "RR", "MENGUKUR", "RESULT", "MONITOR", "TEST",
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
 * The four RR band values are shipped constants, and they are load-bearing twice
 * over: they are what the operator's breath count becomes, and they are fed to
 * the model as TriageInput.respiratory_rate -- whose <= 0 is a REFUSAL gate in
 * tb_classify(). A band mutated to 0 or swapped with its neighbour does not
 * crash and does not show a wrong number anywhere; it silently downgrades real
 * patients or refuses to score them. So each value is pinned against the shipped
 * constant, not re-derived from the switch.
 */
static void test_rr_band_values(void)
{
    assert(ui_rr_band_value(UI_RR_BAND_UNDER_12) == 10U);
    assert(ui_rr_band_value(UI_RR_BAND_12_20) == 16U);
    assert(ui_rr_band_value(UI_RR_BAND_21_30) == 25U);
    assert(ui_rr_band_value(UI_RR_BAND_OVER_30) == 36U);
    printf("rr_band_values OK\n");
}

/*
 * The refusal sentinel and the label it earns. esi 0 is the model's documented
 * "refused to score" output (ui_session.h; tb_classify.h writes it), and it
 * arrives riding UI_PRIORITY_BLACK -- which the Result screen must NOT render
 * as EXPECTANT. Pinned here so a mutation that turns a refusal into a verdict
 * (or vice versa) fails on the host, not on a patient nobody measured.
 */
static void test_verdict_helpers(void)
{
    assert(ui_verdict_unscored(0));
    assert(!ui_verdict_unscored(1));
    assert(!ui_verdict_unscored(5));

    assert(strcmp(ui_verdict_label(UI_PRIORITY_BLACK, 0), "TIDAK TERUKUR") == 0);
    /* A scored verdict keeps its colour's own words. */
    assert(strcmp(ui_verdict_label(UI_PRIORITY_RED, 1),
                  "MERAH - IMMEDIATE") == 0);
    assert(strcmp(ui_verdict_label(UI_PRIORITY_YELLOW, 2),
                  "KUNING - DELAYED") == 0);
    assert(strcmp(ui_verdict_label(UI_PRIORITY_GREEN, 3),
                  "HIJAU - MINOR") == 0);
    printf("verdict_helpers OK\n");
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
    ui_action(UI_SCREEN_RR, 3U);
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
    /* The verdict's numbers as they were at measure-done; the Monitor
     * round-trip below asserts Result still shows exactly these. */
    vitals_t measured;

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
    expect(UI_SCREEN_RR);
    assert(ui_session_has_airway());
    assert(!ui_session_get_airway()); /* "Tidak ada" is the pre-highlighted row */
    ui_action(UI_SCREEN_RR, 3U);
    expect(UI_SCREEN_MENGUKUR);
    assert(ui_session_has_rr());

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
        /* The mock's GREEN carries esi 3, so this walk exercises the SCORED
         * path; the refusal path is pinned in test_verdict_helpers(). */
        assert(!ui_verdict_unscored(ui_session_get_esi()));
    }
    /* The numbers the verdict was computed from, taken here while they are still
     * the only copy in the session -- the baseline for the round-trip below. */
    measured = *ui_session_get_measured_vitals();
    assert((measured.valid_mask & UI_VITAL_HR) != 0U);
    assert((measured.valid_mask & UI_VITAL_SPO2) != 0U);
    printf("measured hr=%u spo2=%u\n",
           (unsigned)measured.hr, (unsigned)measured.spo2);

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

    /*
     * Result must show the MEASUREMENT's numbers, not the live ones: Monitor's
     * per-tick refreshes keep moving session.vitals, so the measured copy latched
     * at measure-done is what keeps the tiles honest after a round-trip. The mock
     * jitters every get, so a live-vs-latched mix-up reads as an assert here
     * rather than as "--" on hardware.
     */
    ui_action(UI_SCREEN_MONITOR, 0U);
    expect(UI_SCREEN_RESULT);
    assert(ui_session_has_priority());
    {
        const vitals_t *m = ui_session_get_measured_vitals();
        assert((m->valid_mask & UI_VITAL_HR) != 0U);
        assert((m->valid_mask & UI_VITAL_SPO2) != 0U);
        /* The latch held: byte-identical to the copy taken at measure-done,
         * while Monitor's live pulls moved session.vitals several times in
         * between. A mutation that makes the live feed reach Result's copy
         * fails here rather than blanking a tile on hardware. */
        assert(m->hr == measured.hr);
        assert(m->spo2 == measured.spo2);
        assert(m->bp_sys == measured.bp_sys);
        assert(m->bp_dia == measured.bp_dia);
        assert(m->rr == measured.rr);
        assert(m->hr_from_ppg == measured.hr_from_ppg);
        printf("result shows measured hr=%u spo2=%u\n",
               (unsigned)m->hr, (unsigned)m->spo2);
    }

    /* Leaving the pair via Home ends the session (Reset). */
    ui_action(UI_SCREEN_RESULT, 1U);
    expect(UI_SCREEN_HOME);
    ui_runtime_tick(3500);
    assert(!ui_session_has_priority());
    assert(!ui_session_has_rfid());

    test_severity_order();
    test_rr_band_values();
    test_verdict_helpers();
    test_monitor_retriage();

    printf("ALL_PASS measure_ms=%u\n", (unsigned)UI_MEASURE_MS);
    return 0;
}
