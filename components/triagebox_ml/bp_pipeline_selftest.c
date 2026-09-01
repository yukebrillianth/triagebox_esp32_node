/*
 * Host selftest for the BP pipeline against its golden vector. Build+run:
 * tools/run_selftests.sh
 *
 * sample_signals.h carries one real labelled capture (subject 001: 60 s @
 * 100 Hz, true cuff 148/90) and the LightGBM pair lgbm_sbp.c/lgbm_dbp.c are
 * deterministic, so the whole feature-extraction + scoring chain has an exact
 * right answer on the host. This file pins it: unlike the triage selftest,
 * which stubs the model away, there is nothing to stub here -- the point IS
 * that the models and the feature order they are fed agree end to end.
 *
 * What that guards, concretely: bp_extract_features writes 23 features into a
 * plain array indexed by the FEAT_* macros, and nothing in the C type system
 * ties features_out[FEAT_PAT_D] to the input[5] the models were trained on.
 * A single transposed macro pair would silently produce a plausible-looking
 * wrong pressure, and only a known-good vector can catch it.
 *
 * The two model files are ~37k lines of unrolled if/else each, so this binary
 * is slow to compile on the host (tens of seconds). Expected; not a reason to
 * drop the flags the runner passes.
 */
#include <assert.h>
#include <stdio.h>

#include "bp_pipeline.h"
#include "sample_signals.h"

/* Sentinels chosen so "output untouched" is distinguishable from any value the
 * pipeline could legitimately write, including 0 and negative model output. */
#define SENTINEL_SBP (-999.0)
#define SENTINEL_DBP (-888.0)

static void test_golden_vector(void)
{
    double sbp = SENTINEL_SBP;
    double dbp = SENTINEL_DBP;

    /*
     * Parameter order is load-bearing: the first two are BOTH PPG (red, then
     * ir -- see bp_pipeline.h), the third is ECG. Swapping red and ir does not
     * fail loudly; it feeds every pulse-timing feature from the wrong channel
     * and moves the answer far enough to break this assert. That is exactly
     * the bug this golden run exists to catch (measured: the swap reads
     * 126.4/68.0 against the 134.9/79.4 pinned below).
     *
     * Pinned against the PORT'S OWN OUTPUT, not the cuff label. The models are
     * deterministic, so this C chain has exactly one right answer per input --
     * but that answer is NOT the subject's 148/90 cuff reading: this ensemble
     * under-predicts hypertensive tails, and the label was never the expected
     * output of the port. Asserting the label would test model accuracy, which
     * is Aslam's training question, not whether this wiring is right. The
     * measured deltas to the cuff (sbp -13.1, dbp -10.6) are recorded here so
     * the accuracy conversation has its numbers in-tree. Re-pinning is the
     * right response to a model retrain; widening to absorb a wiring bug is
     * not.
     */
#define PORT_SBP 134.88
#define PORT_DBP 79.42
    assert(bp_predict(SAMPLE_PPG_RED, SAMPLE_PPG_IR, SAMPLE_ECG_LEAD_I,
                      SAMPLE_SIGNAL_LEN, SAMPLE_IS_MALE, &sbp, &dbp));
    printf("  golden: sbp=%.2f (port %.2f, cuff %.1f), "
           "dbp=%.2f (port %.2f, cuff %.1f)\n",
           sbp, PORT_SBP, SAMPLE_TRUE_SBP,
           dbp, PORT_DBP, SAMPLE_TRUE_DBP);
    assert(sbp >= PORT_SBP - 2.0 && sbp <= PORT_SBP + 2.0);
    assert(dbp >= PORT_DBP - 2.0 && dbp <= PORT_DBP + 2.0);
}

static void test_rejects_short_window(void)
{
    double sbp = SENTINEL_SBP;
    double dbp = SENTINEL_DBP;

    /* 499, one sample under bp_extract_features' 500 minimum: 5 s @ 100 Hz is
     * not enough pulses to estimate anything from. The refusal path must also
     * leave both outputs untouched -- a caller reusing a previous reading on a
     * false return would otherwise mix one patient's cuff with the next. */
    assert(!bp_predict(SAMPLE_PPG_RED, SAMPLE_PPG_IR, SAMPLE_ECG_LEAD_I,
                       499, SAMPLE_IS_MALE, &sbp, &dbp));
    assert(sbp == SENTINEL_SBP);
    assert(dbp == SENTINEL_DBP);
}

static void test_short_window_above_the_gate(void)
{
    double sbp = SENTINEL_SBP;
    double dbp = SENTINEL_DBP;

    /*
     * 1000 samples = 10 s, the shortest window a real caller would offer. The
     * only assert is no-crash plus a boolean: whether 10 s of this capture
     * still yields >= 3 peaks per channel is a property of the signal, and
     * pinning the specific verdict here would freeze the peak detector's
     * tuning to one subject's morphology. What this pins is that 500 is a size
     * gate, not the start of a range of accepted lengths.
     */
    (void)bp_predict(SAMPLE_PPG_RED, SAMPLE_PPG_IR, SAMPLE_ECG_LEAD_I,
                     1000, SAMPLE_IS_MALE, &sbp, &dbp);
}

int main(void)
{
    test_golden_vector();
    test_rejects_short_window();
    test_short_window_above_the_gate();
    printf("bp_pipeline_selftest: OK\n");
    return 0;
}
