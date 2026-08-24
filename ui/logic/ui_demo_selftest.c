/*
 * Host selftest for demo mode. Build+run: tools/run_selftests.sh
 *
 * The thing worth pinning is not that the numbers are pretty, it is that they
 * agree with the RED verdict demo mode hardcodes. A clip showing MERAH beside
 * HR 72 / SpO2 99 is worse than no clip, and nothing else in the tree checks
 * that the two halves of the fake still match.
 */
#include <assert.h>
#include <stdio.h>

#include "ui_demo.h"
#include "ui_status.h"

static void test_flag_defaults_off(void)
{
    /* Off at boot is the safety property: see the header. A device that comes up
     * in demo mode reports RED for a real patient. */
    assert(!ui_demo_enabled());

    ui_demo_toggle();
    assert(ui_demo_enabled());
    ui_demo_toggle();
    assert(!ui_demo_enabled());

    ui_demo_set(true);
    ui_demo_set(true); /* idempotent, unlike toggle */
    assert(ui_demo_enabled());
    ui_demo_set(false);
    assert(!ui_demo_enabled());
}

static void test_vitals_agree_with_red(void)
{
    /* Swept across a full wobble period so the check covers the extremes, not
     * whichever phase happened to be sampled. */
    for (uint32_t t = 0; t < 16000U; t += 100U) {
        vitals_t v = {0};

        ui_demo_vitals(t, &v);

        /* A RED patient: tachycardic, hypoxic, tachypnoeic, hypotensive. Bounds
         * are the clinical direction, not the exact table -- retuning the
         * numbers should not break this, but flipping one the wrong way should. */
        assert(v.hr >= 110 && v.hr <= 150);
        assert(v.spo2 >= 80 && v.spo2 <= 92);
        assert(v.rr >= 25 && v.rr <= 40);
        assert(v.bp_sys >= 70 && v.bp_sys <= 95);
        assert(v.bp_dia < v.bp_sys); /* diastolic below systolic, always */

        /* All four tiles must render, and the all-or-nothing gate must be open:
         * a cleared bit paints "--" and the take is ruined. */
        assert(v.valid_mask == (UI_VITAL_HR | UI_VITAL_SPO2 | UI_VITAL_RR |
                                UI_VITAL_BP));
        assert(v.valid);
    }
}

static void test_vitals_move_but_are_reproducible(void)
{
    vitals_t a = {0};
    vitals_t b = {0};
    vitals_t again = {0};
    bool moved = false;

    ui_demo_vitals(0U, &a);
    ui_demo_vitals(0U, &again);
    /* Same input, same output: the wobble is a table lookup, not a PRNG, which
     * is the only reason the bounds test above can be exhaustive. */
    assert(a.hr == again.hr && a.spo2 == again.spo2 && a.rr == again.rr);

    for (uint32_t t = 0; t < 8U * 800U; t += 800U) {
        ui_demo_vitals(t, &b);
        if (b.hr != a.hr || b.spo2 != a.spo2 || b.rr != a.rr) {
            moved = true;
        }
    }
    /* Four numbers frozen for the whole take look like a screenshot. */
    assert(moved);
}

static void test_battery_is_left_alone(void)
{
    vitals_t v = {0};

    v.battery = 77;
    ui_demo_vitals(1234U, &v);
    /* The gauge reading is real even in demo mode; overwriting it would be a
     * lie with no upside, and the status bar reads the PMIC directly anyway. */
    assert(v.battery == 77);
}

static void test_sensor_mask_is_all_up(void)
{
    /* Exactly UI_SENSOR_ALL: LoRa must NOT be folded in, or the LoRa dot would
     * go green on a box with no radio. It has its own dot for that reason. */
    assert(ui_demo_sensor_mask() == UI_SENSOR_ALL);
    assert((ui_demo_sensor_mask() & UI_SENSOR_LORA) == 0U);
}

static void test_bad_args(void)
{
    ui_demo_vitals(0U, NULL);
}

int main(void)
{
    test_flag_defaults_off();
    test_vitals_agree_with_red();
    test_vitals_move_but_are_reproducible();
    test_battery_is_left_alone();
    test_sensor_mask_is_all_up();
    test_bad_args();
    printf("ui_demo_selftest: OK\n");
    return 0;
}
