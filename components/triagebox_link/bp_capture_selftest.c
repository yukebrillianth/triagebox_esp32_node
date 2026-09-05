/*
 * Host checks for bp_capture's pure parts: the biquad, the ECG heart-rate
 * estimator, and the publish gate. The task/capture side needs FreeRTOS and
 * is exercised on hardware; these three decide whether a reading is published
 * at all, so they are the part worth pinning.
 *
 * Coefficients are re-derived here from the RBJ formulas rather than copied
 * from bp_capture.c, so a mistyped #define fails the frequency checks below
 * instead of quietly shifting a passband.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bp_capture.h"
#include "tb_regs.h" /* tb_bp_pair_valid */
#include "fakes.h"    /* FreeRTOS stand-ins, same pattern as tb_ui_source_selftest */
#include "ui_types.h"
#include "ui_mock.h"

#define FS 100.0
#define PI 3.14159265358979323846

/* --- stubs for everything bp_capture.c calls but the pure tests do not need --
 * the task is never created (xTaskCreate fails), so only the three pure
 * functions and the publish sink matter. */
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack,
                       void *arg, unsigned prio, TaskHandle_t *out)
{
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio;
    if (out != NULL) { *out = NULL; }
    return 0; /* not pdPASS: bp_capture_init() records "task not created" */
}

uint32_t ulTaskNotifyTake(int clear, unsigned wait)
{
    (void)clear; (void)wait;
    return 0U;
}

void xTaskNotifyGive(TaskHandle_t h) { (void)h; }

static bool s_bp_published;
static uint16_t s_bp_pub_sys, s_bp_pub_dia;
void tb_ui_source_on_bp(bool valid, uint16_t sys, uint16_t dia)
{
    (void)valid;
    s_bp_published = true;
    s_bp_pub_sys = sys;
    s_bp_pub_dia = dia;
}

/* The no-ECG BP gate lives in bp_task(), which needs FreeRTOS and is not run
 * here; this keeps the link satisfied. The gate's own reasoning is pinned in
 * tb_ui_source_selftest.c, where the flag it reads is set. */
bool tb_ui_source_ecg_rate_seen(void) { return true; }

bool bp_predict(const double *red, const double *ir, const double *ecg,
                size_t num_samples, double is_male, double *result_sbp,
                double *result_dbp)
{
    (void)red; (void)ir; (void)ecg; (void)num_samples; (void)is_male;
    if (result_sbp != NULL) { *result_sbp = 120.0; }
    if (result_dbp != NULL) { *result_dbp = 80.0; }
    return true;
}

void ui_mock_get_vitals(vitals_t *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->hr = 75U;
        out->valid_mask = UI_VITAL_HR;
    }
}

ui_gender_t ui_session_get_gender(void) { return UI_GENDER_M; }

/*
 * RBJ audio-EQ-cookbook biquad, emitted in the CMSIS convention the runtime
 * expects: {b0, b1, b2, -a1, -a2}, i.e. the two feedback coefficients ALREADY
 * NEGATED.
 *
 * The negation on out[3]/out[4] is the whole point. This function used to emit
 * {b0,b1,b2,+a1,+a2}, which matched bp_biquad_f32()'s then-wrong subtraction, so
 * every frequency assertion below passed while the coefficients actually
 * compiled into the firmware put both poles outside the unit circle. The filter
 * diverged to +-inf inside ~100 samples on hardware and BP never published. A
 * test that derives its inputs in the implementation's own mistake cannot see
 * the mistake -- hence test_coeffs_match_runtime() and test_stability() below,
 * which check the REAL arrays and do not depend on the convention at all.
 */
static void rbj(float f0, float Q, int hp, float out[5])
{
    const double w0 = 2.0 * PI * f0 / FS;
    const double c = cos(w0), s = sin(w0), a = s / (2.0 * Q);
    double b0, b1, b2;
    if (hp) {
        b0 = (1.0 + c) / 2.0; b1 = -(1.0 + c); b2 = (1.0 + c) / 2.0;
    } else {
        b0 = a; b1 = 0.0; b2 = -a;
    }
    const double a0 = 1.0 + a;
    out[0] = (float) (b0 / a0);
    out[1] = (float) (b1 / a0);
    out[2] = (float) (b2 / a0);
    out[3] = (float) (2.0 * c / a0);        /* -a1 */
    out[4] = (float) (-(1.0 - a) / a0);     /* -a2 */
}

/* Feed one signal through, return |y| at the stimulus frequency. */
static double gain_at(const float coeffs[5], double f, unsigned seconds)
{
    float state[2] = {0.0f, 0.0f};
    double last = 0.0, amp = 0.0;
    const unsigned n = (unsigned) (FS * seconds);

    for (unsigned i = 0; i < n; ++i) {
        const double x = sin(2.0 * PI * f * i / FS);
        const double y = bp_biquad_f32((float) x, coeffs, state);
        if (i >= n - 100U) { /* steady-state tail only */
            if (fabs(y) > amp) {
                amp = fabs(y);
            }
            last = 1.0; /* marker: tail reached */
        }
    }
    return last ? amp : amp;
}

static void test_filters(void)
{
    float hp[5], bp[5];
    rbj(0.5f, 0.7071f, 1, hp);
    rbj(8.6603f, 0.866f, 0, bp);

    /* Passband: 1.5 Hz pulse survives the HP; 8 Hz QRS survives the BP. */
    const double hp_pass = gain_at(hp, 1.5, 6);
    assert(hp_pass > 0.85 && hp_pass < 1.15);
    const double bp_pass = gain_at(bp, 8.0, 4);
    assert(bp_pass > 0.85 && bp_pass < 1.15);

    /* Stopband: DC is killed by the HP (< 5% after the transient); 40 Hz is
     * outside the ECG band (< 30%). */
    assert(gain_at(hp, 0.0, 6) < 0.05);
    assert(gain_at(bp, 40.0, 4) < 0.30);

    printf("  filters: HP@1.5Hz=%.2f DC=%.3f, BP@8Hz=%.2f @40Hz=%.2f\n",
           hp_pass, gain_at(hp, 0.0, 6), bp_pass, gain_at(bp, 40.0, 4));
}

/*
 * The arrays the firmware actually runs, against the ones derived above. Both
 * halves of this file were previously self-consistent and still wrong together;
 * this is the check that cannot be fooled that way, because one side is the
 * shipped constant and the other is arithmetic from the filter spec.
 */
static void test_coeffs_match_runtime(void)
{
    float hp[5], bp[5];
    int i;

    rbj(0.5f, 0.7071f, 1, hp);
    rbj(8.6603f, 0.866f, 0, bp);

    for (i = 0; i < 5; ++i) {
        /* 1e-4 absolute: the shipped arrays are written to 6 decimals. */
        assert(fabs((double)(hp[i] - bp_coeff_hp05[i])) < 1e-4);
        assert(fabs((double)(bp[i] - bp_coeff_ecg_bp[i])) < 1e-4);
    }
    printf("  coeffs: bp_coeff_hp05/ecg_bp match the RBJ derivation\n");
}

/*
 * Stability, stated the way the failure actually presented: feed the REAL
 * coefficients a realistic DC-offset input for a full 60 s window and require
 * the output to stay finite and bounded.
 *
 * A frequency-response test cannot catch an unstable filter -- the measured
 * hardware failure had gain inf at every frequency and the response asserts
 * above still passed, because they ran on locally derived coefficients that
 * happened to share the implementation's sign error. This one is convention-
 * agnostic: an unstable biquad diverges no matter how you spell it.
 *
 * 2000 counts of DC is not arbitrary; it is roughly what an idle AD8232 reads on
 * the 12-bit ADC, so it is the input the ECG channel sees when nothing is
 * attached -- exactly the case that produced ecg_span=inf.
 */
static void test_stability(void)
{
    const float *coeffs[2] = {bp_coeff_hp05, bp_coeff_ecg_bp};
    int k;

    for (k = 0; k < 2; ++k) {
        float state[2] = {0.0f, 0.0f};
        float peak = 0.0f;
        int i;

        for (i = 0; i < 6000; ++i) {
            const double x = 2000.0 + 200.0 * sin(2.0 * PI * 1.2 * i / FS);
            const float y = bp_biquad_f32((float) x, coeffs[k], state);

            assert(isfinite(y));
            if (fabsf(y) > peak) {
                peak = fabsf(y);
            }
        }
        /* Bounded by the input's own scale: a stable filter with unity passband
         * cannot amplify a 2200-count input past a few thousand. The old
         * implementation reached 1e22 by sample 50. */
        assert(peak < 5000.0f);
        printf("  stability: ch%d peak|y|=%.1f over 60 s of DC+pulse\n",
               k, (double) peak);
    }
}

/*
 * bp_biquad_prime(): the DC steady state, checked as a fixed point rather than
 * against numbers copied out of the implementation.
 *
 * Two properties, and together they are the whole claim:
 *   1. Feeding constant x0 into a primed filter leaves both the output and the
 *      state where they started -- i.e. it really is the steady state, not a
 *      one-sample patch. NOT that the output is zero: the shipped coefficients
 *      are rounded to six decimals, so the high-pass settles at -1.04e-3 * x0
 *      rather than exactly 0, and a test demanding zero would be testing the
 *      ideal filter instead of the one that ships.
 *   2. A DC step that a zeroed filter rings on for hundreds of samples produces
 *      almost nothing here.
 *
 * Property 2 is the one that mattered on hardware. bp_extract_features()
 * z-scores the whole window, so a 6 s decay from a 160 000-count PPG baseline
 * inflated the divisor 7-9x and find_peaks_1d()'s 0.25 prominence floor stopped
 * seeing the pulse: measured on bp_window.csv, the RED channel found 3 peaks
 * where the same data without the transient gives 74, and every window shorter
 * than 57 s failed the "at least 3 peaks per channel" gate outright.
 */
static void test_prime(void)
{
    const float *coeffs[2] = {bp_coeff_hp05, bp_coeff_ecg_bp};
    const float dc[2] = {160000.0f, 2000.0f}; /* PPG counts, idle AD8232 */
    int k;

    for (k = 0; k < 2; ++k) {
        float primed[2], zeroed[2] = {0.0f, 0.0f};
        float y_first, y_last = 0.0f;
        float peak_zeroed = 0.0f;
        float s_before[2];
        int i;

        bp_biquad_prime(dc[k], coeffs[k], primed);
        s_before[0] = primed[0];
        s_before[1] = primed[1];

        y_first = bp_biquad_f32(dc[k], coeffs[k], primed);
        for (i = 0; i < 600; ++i) { /* 6 s, the measured decay length */
            y_last = bp_biquad_f32(dc[k], coeffs[k], primed);
            const float yz = bp_biquad_f32(dc[k], coeffs[k], zeroed);

            if (fabsf(yz) > peak_zeroed) { peak_zeroed = fabsf(yz); }
        }

        /* (1) fixed point: the output does not move over 6 s of constant input,
         * and the state came back to itself. Tolerances scale with the input
         * because these are absolute ADC counts, not unit steps. */
        assert(fabsf(y_last - y_first) < dc[k] * 1e-5f);
        assert(fabsf(primed[0] - s_before[0]) < dc[k] * 1e-4f);
        assert(fabsf(primed[1] - s_before[1]) < dc[k] * 1e-4f);
        /* And it is small: whatever it settles at is a rounding artefact, not a
         * signal. 1% of the input is far above the measured 0.1% and far below
         * the 100% a zeroed filter starts at. */
        assert(fabsf(y_first) < dc[k] * 0.01f);

        /* (2) the transient it replaces is enormous: the zeroed filter's first
         * output is essentially the whole DC level for the high-pass. Two orders
         * of magnitude apart is the property, not the exact ratio. */
        assert(peak_zeroed > fabsf(y_first) * 100.0f);
        printf("  prime: ch%d DC=%.0f -> steady y=%.4g (drift %.3g over 6s) vs "
               "peak|y|=%.4g zeroed\n", k, (double) dc[k], (double) y_first,
               (double) fabsf(y_last - y_first), (double) peak_zeroed);
    }
}

/* Synthetic ECG: narrow spikes at `bpm`, baseline wobble, 12-bit counts. */
static size_t make_ecg(float *buf, size_t n, double bpm)
{
    const double beat_s = 60.0 / bpm;
    for (size_t i = 0; i < n; ++i) {
        const double t = (double) i / FS;
        double v = 20.0 * sin(2.0 * PI * 0.3 * t); /* baseline wander */
        const double phase = fmod(t, beat_s);
        if (phase < 0.04) { /* 40 ms QRS, ~400 counts tall */
            v += 400.0 * sin(PI * phase / 0.04);
        }
        buf[i] = (float) v;
    }
    return n;
}

static void test_hr(void)
{
    float ecg[6000];
    make_ecg(ecg, 6000, 75.0);
    const uint16_t hr = bp_hr_from_ecg(ecg, 6000);
    assert(hr >= 72 && hr <= 78); /* 60-sample beats: median exact at 75 */
    assert(bp_hr_from_ecg(ecg, 6000) == hr); /* deterministic */

    /* Flat line: no R peaks to find. */
    for (size_t i = 0; i < 6000; ++i) { ecg[i] = 3.0f; }
    assert(bp_hr_from_ecg(ecg, 6000) == 0U);

    printf("  hr: synthetic 75bpm -> %u bpm; flat -> 0\n", hr);
}

static void test_gate(void)
{
    assert(bp_plausible(120.0, 80.0, 75U, 78U, -200.0f, 600.0f));
    assert(!bp_plausible(30.0, 80.0, 75U, 0U, -200.0f, 600.0f)); /* sys low */
    assert(!bp_plausible(120.0, 200.0, 75U, 0U, -200.0f, 600.0f)); /* dia high */
    assert(!bp_plausible(80.0, 120.0, 75U, 0U, -200.0f, 600.0f)); /* dia > sys */
    assert(!bp_plausible(120.0, 80.0, 75U, 0U, 0.0f, 30.0f)); /* flat ECG */
    assert(!bp_plausible(120.0, 80.0, 180U, 100U, -200.0f, 600.0f)); /* hr 80% off */
    assert(bp_plausible(120.0, 80.0, 110U, 100U, -200.0f, 600.0f)); /* within 25% */

    /* Bounds agree with the STM32's own clamp, one link away. */
    assert(tb_bp_pair_valid(120, 80));
    assert(!tb_bp_pair_valid(39, 20));
    assert(!tb_bp_pair_valid(261, 180));

    printf("  gate: ranges + flat-ECG + HR agreement all as pinned\n");
}

/*
 * The console entry points against a live capture, observed through the one
 * thing visible from outside: whether the capture self-closes when the pushed
 * samples hit the record target. The stubbed xTaskCreate keeps the BP task
 * inert, so the statics are only moved by what the test drives.
 */
static void test_record_refusal(void)
{
    /* Refused while a measurement runs: neither command may arm, so the
     * pushes below never hit a record target and the patient's window is
     * never closed (or truncated at 3000) out from under them. A regression
     * here would call start() inside record(), zeroing s_count and arming
     * the target -- the capture would then self-close mid-run and this
     * assert dies. */
    bp_capture_start();
    bp_capture_record(30U);
    bp_capture_arm_dump();
    for (uint32_t i = 0; i < 6000U; ++i) { /* the accumulator's full capacity */
        bp_capture_wave_push(0U, 0U, 0U);
    }
    assert(bp_capture_capturing());
    bp_capture_measure_done();

    /* Control, idle: the same command arms, and the capture self-closes at
     * exactly 30 s x 100 Hz = 3000 samples. */
    bp_capture_record(30U);
    assert(bp_capture_capturing());
    for (uint32_t i = 0; i < 2999U; ++i) {
        bp_capture_wave_push(0U, 0U, 0U);
    }
    assert(bp_capture_capturing());
    bp_capture_wave_push(0U, 0U, 0U);
    assert(!bp_capture_capturing());
    printf("  record: refused mid-measure; self-closes at 3000 when idle\n");
}

int main(void)
{
    /* The xTaskCreate stub refuses: init must say so (esp_err_t) rather than
     * pretend the BP task exists. */
    assert(bp_capture_init() != ESP_OK);
    /* An empty window has no beats -- and must not read ecg[0]: the real
     * caller reaches this over a malloc(0), which may hand back NULL. */
    assert(bp_hr_from_ecg(NULL, 0U) == 0U);
    test_filters();
    test_coeffs_match_runtime();
    test_stability();
    test_prime();
    test_hr();
    test_gate();
    test_record_refusal();
    printf("bp_capture_selftest: OK\n");
    return 0;
}
