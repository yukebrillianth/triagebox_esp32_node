/*
 * Host selftest for the triage adapter. Build+run: tools/run_selftests.sh
 *
 * Two things are pinned here, and the second is the reason this file is worth
 * having at all.
 *
 * 1. The band conversions in tb_triage.c.
 *
 * 2. The ESI -> START colour mapping in the ML side's tb_classify.h. That is the
 *    highest-consequence arithmetic in the firmware: get it wrong and the most
 *    critical patient is shown as the least urgent. It is tested by including
 *    tb_classify.h and supplying our OWN predict_triage(), so the mapping is
 *    checked without linking the 72k-line model -- the reason it went untested
 *    before. The stub is scripted through s_next_esi.
 *
 * Nothing here tests the model's accuracy. That is a training question and no
 * amount of C can answer it.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tb_triage.h"
#include "triage_pipeline.h" /* TriageInput/TriageOutput; declarations only */

/* Scripted stand-in for the 72k-line GBM, so the mapping above it is testable.
 * Defined here rather than linked, which is the whole trick: tb_classify.h calls
 * predict_triage() and does not care that the real one is not in this binary. */
static int s_next_esi = 3;
static int s_predict_calls;

TriageOutput predict_triage(const TriageInput *in)
{
    TriageOutput out;

    (void)in;
    ++s_predict_calls;
    memset(&out, 0, sizeof(out));
    out.predicted_esi = s_next_esi;
    if ((s_next_esi >= 1) && (s_next_esi <= 5)) {
        /* A probability the confidence path can be checked against: distinct per
         * class, so an off-by-one in probs[esi - 1] shows up as a wrong number
         * rather than a plausible one. */
        out.probs[s_next_esi - 1] = 0.1f * (float)s_next_esi;
    }
    return out;
}

#include "tb_classify.h" /* the mapping under test, body and all */

static TriageInput healthy(void)
{
    TriageInput in;

    in.age = 31.0f;
    in.sex = 1.0f;
    in.systolic_bp = 120.0f;
    in.heart_rate = 80.0f;
    in.respiratory_rate = 16.0f;
    in.spo2 = 98.0f;
    in.airway_problem = 0;
    return in;
}

static void test_esi_mapping(void)
{
    TriageInput in = healthy();
    int esi = -1;

    /*
     * Indonesian START uses three colours, so the five ESI classes collapse:
     *   ESI 1       -> RED     (resuscitation)
     *   ESI 2       -> YELLOW  (emergent)
     *   ESI 3, 4, 5 -> GREEN   (urgent / less urgent / non-urgent)
     * This is the ML side's grouping, not this file's -- it is asserted here so
     * that changing it is a deliberate act with a failing test, not a silent
     * edit inside a header.
     */
    s_next_esi = 1;
    assert(tb_classify(&in, &esi) == UI_PRIORITY_RED);
    assert(esi == 1);
    s_next_esi = 2;
    assert(tb_classify(&in, &esi) == UI_PRIORITY_YELLOW);
    assert(esi == 2);
    s_next_esi = 3;
    assert(tb_classify(&in, &esi) == UI_PRIORITY_GREEN);
    assert(esi == 3);
    s_next_esi = 4;
    assert(tb_classify(&in, &esi) == UI_PRIORITY_GREEN);
    assert(esi == 4);
    s_next_esi = 5;
    assert(tb_classify(&in, &esi) == UI_PRIORITY_GREEN);
    assert(esi == 5);

    /* The critical direction as a property: a lower ESI is never a less urgent
     * colour than a higher one. ui_priority_t happens to be in severity order
     * for the three used here (RED=0 < YELLOW=1 < GREEN=2), which is what makes
     * the comparison meaningful -- BLACK=3 is not on this scale and is tested
     * separately below. */
    for (int a = 1; a <= 5; a++) {
        for (int b = a + 1; b <= 5; b++) {
            ui_priority_t pa;
            ui_priority_t pb;

            s_next_esi = a;
            pa = tb_classify(&in, NULL);
            s_next_esi = b;
            pb = tb_classify(&in, NULL);
            assert(pa <= pb);
        }
    }
}

static void test_esi_is_written_through_the_pointer(void)
{
    TriageInput in = healthy();
    int esi = 4242;

    /* The bug this guards: `predicted_esi = output.predicted_esi;` assigns to
     * the pointer instead of through it, so the caller's variable keeps whatever
     * it was initialised with and every ESI ever logged is garbage. */
    s_next_esi = 2;
    (void)tb_classify(&in, &esi);
    assert(esi == 2);

    /* And NULL must be accepted, since the colour is often all a caller wants. */
    (void)tb_classify(&in, NULL);
}

static void test_refuses_on_missing_features(void)
{
    static const char *names[] = {"hr", "rr", "spo2", "sbp"};
    unsigned i;

    s_next_esi = 1; /* would be RED if the model were consulted at all */

    for (i = 0; i < 4U; i++) {
        TriageInput in = healthy();
        int esi = -1;
        int before = s_predict_calls;

        switch (i) {
        case 0: in.heart_rate = 0.0f; break;
        case 1: in.respiratory_rate = 0.0f; break;
        case 2: in.spo2 = 0.0f; break;
        default: in.systolic_bp = 0.0f; break;
        }

        assert(tb_classify(&in, &esi) == UI_PRIORITY_BLACK);
        /* 0, not 1. The ESI scale starts at 1 = resuscitation, so reporting 1
         * for a patient nobody measured claims the most critical class. */
        assert(esi == 0);
        /* And the model must not be run at all on absent features. */
        assert(s_predict_calls == before);
        (void)names[i];
    }
}

static void test_airway_problem_forces_red(void)
{
    TriageInput in = healthy();
    int esi = -1;

    /* The one override: an airway problem is RED whatever the model said. Note
     * the ESI still reports the model's own answer, which is the point of
     * exposing both -- RED with esi=5 is the operator's override, RED with esi=1
     * is the model, and the log has to be able to tell them apart. */
    in.airway_problem = 1;
    s_next_esi = 5;
    assert(tb_classify(&in, &esi) == UI_PRIORITY_RED);
    assert(esi == 5);

    /* Every ESI, since the override has to win over all of them and not just
     * over the GREEN band. */
    for (int e = 1; e <= 5; e++) {
        s_next_esi = e;
        assert(tb_classify(&in, NULL) == UI_PRIORITY_RED);
    }

    /* But it does NOT rescue a refusal into RED. With no vitals the box does not
     * know what it is looking at, and an operator who has seen an obstructed
     * airway does not need the screen's permission to act. */
    in.heart_rate = 0.0f;
    s_next_esi = 3;
    assert(tb_classify(&in, &esi) == UI_PRIORITY_BLACK);
    assert(esi == 0);
}

static void test_age_bands(void)
{
    /* Midpoints, and strictly increasing -- an older band must never yield a
     * younger number, which is the kind of transposition a table invites. */
    assert(tb_triage_age_years(UI_AGE_BAND_6_17) == 12.0f);
    assert(tb_triage_age_years(UI_AGE_BAND_18_45) == 31.0f);
    assert(tb_triage_age_years(UI_AGE_BAND_46_60) == 53.0f);
    assert(tb_triage_age_years(UI_AGE_BAND_OVER_60) == 70.0f);

    assert(tb_triage_age_years(UI_AGE_BAND_6_17) <
           tb_triage_age_years(UI_AGE_BAND_18_45));
    assert(tb_triage_age_years(UI_AGE_BAND_18_45) <
           tb_triage_age_years(UI_AGE_BAND_46_60));
    assert(tb_triage_age_years(UI_AGE_BAND_46_60) <
           tb_triage_age_years(UI_AGE_BAND_OVER_60));

    /* Each midpoint must sit inside its own band, or the "midpoint" claim is
     * just a number. */
    assert(tb_triage_age_years(UI_AGE_BAND_6_17) >= 6.0f &&
           tb_triage_age_years(UI_AGE_BAND_6_17) <= 17.0f);
    assert(tb_triage_age_years(UI_AGE_BAND_18_45) >= 18.0f &&
           tb_triage_age_years(UI_AGE_BAND_18_45) <= 45.0f);
    assert(tb_triage_age_years(UI_AGE_BAND_46_60) >= 46.0f &&
           tb_triage_age_years(UI_AGE_BAND_46_60) <= 60.0f);
    assert(tb_triage_age_years(UI_AGE_BAND_OVER_60) > 60.0f);

    /* An out-of-range band is an upstream bug, not a newborn. */
    assert(tb_triage_age_years((ui_age_band_t)99) == 31.0f);
}

static void test_sex(void)
{
    assert(tb_triage_sex(UI_GENDER_M) == 1.0f);
    assert(tb_triage_sex(UI_GENDER_F) == 0.0f);
    /* Strictly between, not defaulted to either: an unentered sex must not be
     * asserted as one of them. */
    assert(tb_triage_sex(UI_GENDER_U) > tb_triage_sex(UI_GENDER_F));
    assert(tb_triage_sex(UI_GENDER_U) < tb_triage_sex(UI_GENDER_M));
}

int main(void)
{
    test_esi_mapping();
    test_esi_is_written_through_the_pointer();
    test_refuses_on_missing_features();
    test_airway_problem_forces_red();
    test_age_bands();
    test_sex();
    printf("tb_triage_selftest: OK\n");
    return 0;
}
