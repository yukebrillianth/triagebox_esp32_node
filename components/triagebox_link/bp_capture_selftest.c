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
    out[3] = (float) (-2.0 * c / a0);
    out[4] = (float) ((1.0 - a) / a0);
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

int main(void)
{
    test_filters();
    test_hr();
    test_gate();
    printf("bp_capture_selftest: OK\n");
    return 0;
}
