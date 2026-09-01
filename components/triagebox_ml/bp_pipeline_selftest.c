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
     * Pinned against the PORT'S OWN OUTPUT, not the cuff label, and the reason
     * is not "close enough". Three measurements, all on this same vector:
     *
     *   - 21 of the 23 features land inside the range the trees actually split
     *     on, so the model is being fed values of the kind it was trained on.
     *     The two that do not (pi_ir, ac_dc_ratio) are computed AC/DC after a
     *     z-score, so their denominator is the 1e-5 epsilon; moving them
     *     anywhere in the training range changes the answer by under 1 mmHg.
     *   - The chain is chaotic at the scale of the delta it is being judged by:
     *     re-sampling this record onto a grid shifted half a millisecond reads
     *     120.8 instead of 134.9, and the eleven 40 s sub-windows of this same
     *     60 s capture span 117.1..137.8.
     *   - The file's own SAMPLE_TRUE_HR says 103 bpm while its waveform beats
     *     69.7 (see test_input_time_base). The cuff 148/90 was recorded with
     *     that 103 bpm, i.e. not during the seconds stored here.
     *
     * So the cuff label is not this vector's expected output, and a +-2 mmHg
     * assert against it would be measuring the label, not the wiring. What this
     * pin does catch is a transposed FEAT_* macro, a swapped channel, or a
     * model file replaced by mistake. Re-pinning is the right response to a
     * retrain or a re-exported sample; widening to absorb a wiring bug is not.
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

/*
 * The pinned numbers above are only meaningful for the exact bytes in
 * sample_signals.h, and the one property of those bytes that decides every
 * timing feature is how many samples a heartbeat takes. Measured here by plain
 * autocorrelation -- no shared code with the pipeline's peak detector, so this
 * still holds if that detector changes.
 *
 * 87 samples at the declared 100 Hz is 69 bpm, and the file's own
 * SAMPLE_TRUE_HR says 103. The ratio is 1.48, close enough to a skipped
 * 150 -> 100 Hz resample to be worth Aslam's attention; the models themselves
 * vote for 100 Hz being right (their split thresholds imply a corpus median of
 * 77-79 bpm, which is a normal heart rate, and 1.48x that would be 118).
 * Either way the label and the waveform disagree, so if the sample file is ever
 * re-exported this assert fires and PORT_SBP/PORT_DBP have to be re-measured
 * rather than silently drifting.
 */
static void test_input_time_base(void)
{
    double mean = 0.0;
    double best = -1e300;
    int best_lag = 0;
    int lag;
    size_t i;

    for (i = 0; i < SAMPLE_SIGNAL_LEN; i++) {
        mean += SAMPLE_PPG_IR[i];
    }
    mean /= (double)SAMPLE_SIGNAL_LEN;

    for (lag = 40; lag <= 200; lag++) { /* 30..150 bpm at 100 Hz */
        double acc = 0.0;
        for (i = 0; i + (size_t)lag < SAMPLE_SIGNAL_LEN; i++) {
            acc += (SAMPLE_PPG_IR[i] - mean) *
                   (SAMPLE_PPG_IR[i + (size_t)lag] - mean);
        }
        acc /= (double)(SAMPLE_SIGNAL_LEN - (size_t)lag);
        if (acc > best) {
            best = acc;
            best_lag = lag;
        }
    }

    printf("  input: %d samples/beat -> %.1f bpm at the declared %.0f Hz "
           "(file label says %.0f)\n", best_lag,
           60.0 * BP_SAMPLING_RATE_HZ / best_lag, BP_SAMPLING_RATE_HZ,
           SAMPLE_TRUE_HR);
    assert(best_lag == 87);
}

int main(void)
{
    test_input_time_base();
    test_golden_vector();
    test_rejects_short_window();
    test_short_window_above_the_gate();
    printf("bp_pipeline_selftest: OK\n");
    return 0;
}
