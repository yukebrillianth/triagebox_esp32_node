/*
 * Host selftest for the ESI -> START colour mapping and the vitals window.
 * Build+run: tools/run_selftests.sh
 *
 * The mapping is the highest-consequence line of arithmetic in the firmware: get
 * it wrong and the most critical patient is shown as the least urgent. It is
 * pinned here rather than trusted to review.
 *
 * tb_triage_classify() itself is not exercised: it pulls in the whole 49k-line
 * GBM pipeline. This covers the two conversions the UI depends on.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tb_triage.h"

static void test_esi_mapping(void)
{
    /* ESI runs 1 (resuscitation) .. 5 (non-urgent). ui_priority_t is NOT in
     * severity order (RED=0, YELLOW=1, GREEN=2, BLACK=3), so a plain cast --
     * which is what tb_classify.h still does -- maps ESI 1 to YELLOW. */
    assert(tb_triage_esi_to_priority(1) == UI_PRIORITY_RED);
    assert(tb_triage_esi_to_priority(2) == UI_PRIORITY_RED);
    assert(tb_triage_esi_to_priority(3) == UI_PRIORITY_YELLOW);
    assert(tb_triage_esi_to_priority(4) == UI_PRIORITY_GREEN);
    assert(tb_triage_esi_to_priority(5) == UI_PRIORITY_GREEN);

    /* 0 is the pipeline's "cannot score" sentinel. */
    assert(tb_triage_esi_to_priority(0) == UI_PRIORITY_BLACK);

    /* Anything out of range is an upstream bug and must never surface as a
     * treatable colour. */
    assert(tb_triage_esi_to_priority(6) == UI_PRIORITY_BLACK);
    assert(tb_triage_esi_to_priority(-1) == UI_PRIORITY_BLACK);
    assert(tb_triage_esi_to_priority(9999) == UI_PRIORITY_BLACK);

    /* The critical direction, stated as a property: a lower ESI is never a
     * less urgent colour than a higher one. */
    for (int a = 1; a <= 5; a++) {
        for (int b = a + 1; b <= 5; b++) {
            /* Severity rank, most urgent first. */
            static const int rank[4] = {0, 1, 2, 3}; /* RED,YELLOW,GREEN,BLACK */
            int ra = rank[tb_triage_esi_to_priority(a)];
            int rb = rank[tb_triage_esi_to_priority(b)];
            assert(ra <= rb);
        }
    }
}

static vitals_t sample(uint16_t hr, uint16_t spo2, uint16_t rr, uint16_t sys)
{
    vitals_t v = {
        .hr = hr, .spo2 = spo2, .rr = rr, .bp_sys = sys, .bp_dia = 70,
        .battery = 80,
        .valid_mask = UI_VITAL_HR | UI_VITAL_SPO2 | UI_VITAL_RR | UI_VITAL_BP,
        .valid = true,
    };
    return v;
}

static void test_window_aggregates(void)
{
    tb_vitals_window_t w;
    vitals_t a = sample(80, 98, 16, 120);
    vitals_t b = sample(120, 92, 24, 100);
    vitals_t c = sample(100, 95, 20, 110);

    tb_vitals_window_reset(&w);
    assert(w.samples == 0);

    tb_vitals_window_add(&w, &a);
    /* The first sample must seed both ends: a zeroed struct would otherwise
     * pin every minimum at 0, and 0 is what the model reads as "no sensor". */
    assert(w.hr_min == 80 && w.hr_max == 80);

    tb_vitals_window_add(&w, &b);
    tb_vitals_window_add(&w, &c);

    assert(w.samples == 3);
    assert(w.hr_min == 80 && w.hr_max == 120);
    assert(w.spo2_min == 92 && w.spo2_max == 98);
    assert(w.rr_min == 16 && w.rr_max == 24);
    assert(w.sbp_min == 100 && w.sbp_max == 120);
    assert(w.hr_sum == 300); /* mean 100 */
}

static void test_window_ignores_invalid(void)
{
    tb_vitals_window_t w;
    vitals_t good = sample(80, 98, 16, 120);
    vitals_t stale = sample(0, 0, 0, 0);

    stale.valid = false;

    tb_vitals_window_reset(&w);
    tb_vitals_window_add(&w, &stale);
    assert(w.samples == 0); /* a stale frame must not become a data point */

    tb_vitals_window_add(&w, &good);
    tb_vitals_window_add(&w, &stale);
    assert(w.samples == 1);
    /* Crucially the zeros must not have dragged the minimum down: that would
     * silently trip the model's "sensor missing" gate on a healthy patient. */
    assert(w.hr_min == 80 && w.spo2_min == 98);
}

static void test_bad_args(void)
{
    tb_vitals_window_t w;
    vitals_t v = sample(80, 98, 16, 120);

    tb_vitals_window_reset(NULL);
    tb_vitals_window_add(NULL, &v);
    tb_vitals_window_reset(&w);
    tb_vitals_window_add(&w, NULL);
    assert(w.samples == 0);
}

int main(void)
{
    test_esi_mapping();
    test_window_aggregates();
    test_window_ignores_invalid();
    test_bad_args();
    printf("tb_triage_selftest: OK\n");
    return 0;
}
