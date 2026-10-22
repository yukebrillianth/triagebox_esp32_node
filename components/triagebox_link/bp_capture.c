#include "bp_capture.h"

#include <math.h>
#include <stdio.h> /* printf: the CSV dump goes straight to the console */
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* test_fakes/fakes.h (host selftests) spells esp_err_t/ESP_OK but not the
 * error constants yet; that file belongs to another agent and has been
 * reported. The guard drops out once they add the real one -- the value is
 * the IDF header's own. */
#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM 0x101
#endif

#include "bp_pipeline.h"
#include "tb_regs.h"
#include "tb_ui_source.h"
#include "ui_mock.h"
#include "ui_session.h"
#include "ui_types.h"

static const char *TAG = "bp_capture";

/* The model was trained on 60 s @ 100 Hz (sample_signals.h: 6000 samples), and
 * the accumulator is still sized for that so `bplog 60` can record a full
 * minute. UI_MEASURE_MS is 45 s now (see ui_mock.h for the measurement behind
 * that), so a measurement fills ~4453 of these; a longer one stops appending
 * rather than wrapping. */
#define BP_WINDOW_SAMPLES 6000U

/*
 * FS as the code uses it. TWO numbers are honest here because they are used
 * differently: BP_FS_HZ (100.0f) sizes RECORD windows -- seconds x samples via
 * `want = seconds * BP_FS_HZ` below, and BP_SAMPLES_IN_MS's true 98.97 Hz then
 * only shifts when a recorded window self-closes, by <1 s at 60 s; it also
 * converts peak intervals to bpm in bp_hr_from_ecg, where the true rate would
 * read LOW by ~1% (74.2 vs 75.0 -- under a bpm, vs the +/-25% agreement gate
 * that consumes it). Keeping the round 100.0f is deliberate, not stale: every
 * coefficient and golden vector in this repo was derived at FS=100.
 * ponytail: if HR ever feeds a stricter consumer, switch bp_hr_from_ecg to
 * 98.97f and re-pin the bp_window golden.
 */
#define BP_FS_HZ 100.0f

/*
 * The floor for RUNNING the model, which is not the same number as the window
 * size -- and conflating the two is why this never produced a reading.
 *
 * Requiring a full window inside the measure time demands more than 100 Hz, and
 * the source does not deliver it: the MAX30102 at sr_800/smp_ave_8 is nominally
 * 100 Hz but its internal oscillator is spec'd to a few percent, and the push is
 * paced by the STM32 superloop on top of that. Measured on hardware: 5937
 * samples in 59 990 ms = 98.97 Hz. So an exact-count gate always arrived ~60
 * samples early and the window was always rejected, however clean the capture.
 *
 * 3000 = 30 s: one dropped wave sample restarts the ENTIRE accumulator
 * (tb_link_i2c.c), so this floor is really the gap tolerance the capture
 * survives, and 4000 left only ~4.6 s of it against the 45 s window's expected
 * ~4453 (98.97 Hz); 3000 restores ~14.7 s. Refusing any earlier is still
 * honest: on the one recording swept (tools/bp_window_sweep.c), 30 s reads
 * +3.4/-4.7 mmHg against the 60 s reference, and the triage's 129.7 mmHg
 * imputation covers the patient past that. The _Static_asserts below bind it
 * to the window -- if either constant moves, they say whether the pair still
 * works instead of leaving BP quietly unpublishable.
 */
#define BP_MIN_SAMPLES 3000U

/* The on-device rate: measured 5937 samples in 59 990 ms. Integer 100ths of
 * Hz, exact -- not a rounded fraction of a rounded 98.97f. */
#define BP_FS_HZ100 9897U

/* Expected samples in a window of `ms` milliseconds, at the real rate.
 * Integer so it can be evaluated in the _Static_asserts below. */
#define BP_SAMPLES_IN_MS(ms) ((uint32_t)(ms) * BP_FS_HZ100 / 100000U)

/*
 * The window/floor invariants, checked for real at compile time. They bind to
 * UI_MEASURE_MS_DEFAULT, not UI_MEASURE_MS, on purpose: -DUI_MEASURE_MS=2000
 * (run_selftests.sh, sim) accelerates the desktop UI and cannot displace the
 * hardware window the floor must fill -- so the asserts read identically in
 * both builds, and the host never scores BP anyway (xTaskCreate fails there).
 *
 * (1) The floor must be reachable inside the default window, with the gap
 *     tolerance that is the floor's whole purpose: this is the gate that ran
 *     for weeks as 4800-of-5937 -- BP silently never published, triage
 *     imputed 129.7 for every patient, and the box still looked like it
 *     worked. (2) is the form that failure takes after a partial fix: 4000
 *     of 4453 left only 4.6 s of tolerance and this assert refuses it.
 * (3) The accumulator must still hold the longest window bp_capture_record()
 *     may be asked for -- `bplog 60`, the training length -- at the true
 *     98.97 Hz.
 */
_Static_assert(BP_MIN_SAMPLES < BP_SAMPLES_IN_MS(UI_MEASURE_MS_DEFAULT),
               "BP floor no longer reachable inside the hardware window");
_Static_assert(BP_SAMPLES_IN_MS(UI_MEASURE_MS_DEFAULT) - BP_MIN_SAMPLES >=
                   BP_SAMPLES_IN_MS(10000U),
               "gap tolerance under 10 s: one dropped sample can starve BP");
_Static_assert(BP_WINDOW_SAMPLES >= BP_SAMPLES_IN_MS(60000U),
               "accumulator no longer holds a 60 s bplog window");

/* 3 channels x 6000 x 2 B = 36 KB of internal BSS. PSRAM is not used here on
 * purpose: the push site is the poll task at 20 Hz writing three stores, and
 * the read side runs once per measurement.
 *
 * WIRE VALUES, PPG unsigned, ECG effectively signed. The PPG half held
 * `(int16_t) tb_ppg_unpack(v)` until 2026-09-04, which is mod-65536 arithmetic
 * on an 18-bit number: harmless as a constant offset for most DC levels (the
 * 0.5 Hz high-pass removes it), but a PPG baseline sitting near 32768, 98304,
 * 163840 or 229376 counts puts the pulse across the wrap and every straddling
 * sample jumps by 65536. Storing the wire form -- counts >> TB_PPG_SHIFT, which
 * is how the STM32 sent it and is exactly representable -- removes the hazard
 * instead of hoping the baseline lands away from it. tb_ppg_unpack() below
 * restores counts losslessly. The ECG half is read back through (int16_t) at
 * the filter -- see there for why the sign is load-bearing. */
static uint16_t s_acc[3][BP_WINDOW_SAMPLES];
static uint32_t s_count; /* samples appended (all three channels together) */
static bool s_capturing;

/*
 * One-shot CSV dump of the next completed window, raw and filtered, armed from
 * the debug console (`bplog`) so the RBJ biquads here can be compared offline
 * against the friend's Butterworth (HP o3 0.5 Hz + LP o4 6 Hz, Fs 100 -- corner
 * frequencies read straight off the shipped coefficients). ~6000 lines at
 * 115200 baud is ~30 s of print, AFTER the verdict, from this task only.
 *
 * Raw is what the filter decision needs: any candidate filter can be re-run on
 * the raw column offline (bp_pipeline.c compiles on the host), and features and
 * scores recomputed under both, which no filtered-only dump allows.
 */
static volatile bool s_dump_armed;

/*
 * Record mode: capture a window WITHOUT a patient, a measurement or a screen,
 * for signal analysis (`bplog <seconds>`).
 *
 * Worth its own mode because the two questions are different. Publishing a BP
 * needs a real measurement and refuses without electrodes; looking at the
 * waveform needs neither, and tying data collection to a working model means no
 * data exactly when the model is what is being debugged. The STM32 pushes into
 * its ring unconditionally (see TB_FLAG_PPG_CONTACT in tb_regs.h), so there is
 * nothing to ask it for.
 *
 * The model still runs and its numbers still reach the CSV header -- that is
 * information. What is skipped is the PUBLISH: tb_link_send_bp() would stamp
 * this pressure into the STM32's LoRa packet for whichever patient it last knew.
 */
static volatile bool s_record_only;
/* Sample count bp_capture_record() asked for; 0 = a normal dump (measure-made window). */
static volatile uint32_t s_record_target;

/* Biquad state, per channel. Re-zeroed at each start. */
static float s_state_red[2], s_state_ir[2], s_state_ecg[2];

/*
 * Filter coefficients from the RBJ audio-EQ-cookbook biquad formulas at
 * FS = 100 Hz (w0 = 2*pi*f0/FS, alpha = sin(w0)/(2Q); HP: b0=(1+cos w0)/2,
 * b1=-(1+cos w0), b2=(1+cos w0)/2, a1=-2 cos w0, a2=1-alpha, all over
 * a0 = 1+alpha; bandpass likewise).
 *
 * PPG: high-pass 0.5 Hz, Q 0.707. The training waveforms are bandpassed and
 * bp_extract_features z-scores whatever it gets -- but it computes AC/DC
 * features from those z-scored signals, so a DC term that training never saw
 * would shift every one of them. Remove the DC, keep the pulse.
 *
 * ECG: bandpass 5-15 Hz, Q 0.866, for R-peak TIMING only. The STM32's own DSP
 * runs the same band at 497.5 Hz for its bpm work; this is the same passband
 * at the wire's 100 Hz so the peak timing agrees.
 */
const float bp_coeff_hp05[5] = {0.978027f, -1.956055f, 0.978027f,
                                1.955572f, -0.956537f};
const float bp_coeff_ecg_bp[5] = {0.230114f, 0.0f, -0.230114f,
                                  1.317384f, -0.539771f};

/* Flat-line floor for the ECG gate, in filtered ADC counts. A lead-off trace
 * is a constant plus a few counts of noise; a real QRS at 12-bit gain clears
 * hundreds. (True hardware lead-off needs the AD8232 LO pin -- filed, not
 * wired; this gate is the software stand-in.) */
#define BP_ECG_FLAT_FLOOR 50.0f

static TaskHandle_t s_task;

/*------------------------ pure helpers (host-tested) ----------------------*/

float bp_biquad_f32(float x, const float coeffs[5], float state[2])
{
    /*
     * coeffs = {b0, b1, b2, -a1, -a2} -- the CMSIS convention, where the two
     * feedback terms arrive ALREADY NEGATED, so they are ADDED here. Subtracting
     * them (as this did until 2026-09-02) negates a1 and a2 a second time, which
     * puts both poles outside the unit circle: the filter diverges to +-inf
     * within about 100 samples. On hardware that showed up as
     * "BP implausible: sbp=0.0 dia=0.0 ecg_span=inf" -- every measurement, with a
     * clean 60 s capture, because bp_predict() was fed infinities.
     */
    const float y = coeffs[0] * x + state[0];
    state[0] = coeffs[1] * x + coeffs[3] * y + state[1];
    state[1] = coeffs[2] * x + coeffs[4] * y;
    return y;
}

void bp_biquad_prime(float x0, const float coeffs[5], float state[2])
{
    /*
     * Start the filter in its DC steady state for input x0, instead of at zero.
     *
     * THIS IS NOT COSMETIC, and the measurement says so. A PPG channel sits at
     * ~160 000 counts of DC; a zeroed state means sample 0 is a full-scale step,
     * and the 0.5 Hz high-pass takes ~6 s (600 samples) to decay through it.
     * bp_extract_features() z-scores the WHOLE window, so those 600 samples
     * inflate the standard deviation it divides by: measured on bp_window.csv,
     * sd(all)/sd(after 6 s) is 7.2x on IR and 9.0x on RED. The pulse is then
     * 7-9x smaller in z units than it really is, and find_peaks_1d()'s 0.25
     * prominence floor stops seeing it -- the RED channel found 3 peaks in the
     * 57.8 s window against 74 without the transient, and 2 (a refusal: the gate
     * is >= 3 per channel) at every shorter length. So the transient, not the
     * signal, is what made a shorter measurement impossible and every full one
     * marginal. With this, the same recording scores at every length from 30 s
     * up, SBP within 3.4 mmHg of the 60 s answer.
     *
     * Solved from the recurrence rather than assumed: with y and x both constant,
     *   y = b0*x + s0,  s0 = b1*x + c3*y + s1,  s1 = b2*x + c4*y
     * gives y = x*(b0+b1+b2)/(1 - c3 - c4), and the two states follow. That is an
     * exact fixed point for the coefficients AS WRITTEN, which matters: they are
     * rounded to six decimals, so b0+b1+b2 is -1e-6 rather than 0 and the
     * high-pass really does settle at -1.04e-3 * x0 (about -166 counts here), not
     * at zero. Assuming the ideal zero would leave a 166-count step to ring on --
     * 950x smaller than the bug, but the same shape. A constant offset is free
     * anyway: the z-score subtracts the mean.
     *
     * The first sample is still the honest first sample; what is removed is the
     * filter's memory of a signal that was never there.
     */
    const float denom = 1.0f - coeffs[3] - coeffs[4];
    float y0;

    if (fabsf(denom) < 1e-9f) {
        /* A pole at z = 1: an integrator, which has no DC steady state to prime
         * to. Neither shipped filter is one (denom is 9.65e-4 and 0.222), so this
         * is only here so a future coefficient change degrades to the old
         * behaviour instead of dividing by zero. */
        state[0] = 0.0f;
        state[1] = 0.0f;
        return;
    }
    y0 = x0 * (coeffs[0] + coeffs[1] + coeffs[2]) / denom;
    state[1] = coeffs[2] * x0 + coeffs[4] * y0;
    state[0] = y0 - coeffs[0] * x0;
}

uint16_t bp_hr_from_ecg(const float *ecg, size_t n)
{
    /* Peaks at most every 250 ms (25 samples): a heart faster than 240 bpm is
     * not a heart this box can measure anyway, and the cap bounds the arrays.
     * 6000/25 = 240 slots, sized off the window constant. */
    uint32_t peaks[240];
    uint32_t intervals[240];
    size_t np = 0, ni = 0, i;
    float max, min;
    uint32_t median;

    if ((n == 0U) || (ecg == NULL)) {
        return 0U; /* an empty window has no beats -- and ecg[0] below would
                    * read past a malloc(0): `bplog <n>` on a link with no
                    * wave data reaches here over the capture's n-sized alloc */
    }
    max = min = ecg[0];

    for (i = 1U; i < n; ++i) {
        if (ecg[i] > max) { max = ecg[i]; }
        if (ecg[i] < min) { min = ecg[i]; }
    }
    if ((max - min) < BP_ECG_FLAT_FLOOR) {
        return 0U;
    }

    {
        const float thresh = min + (max - min) * 0.6f;
        uint32_t last = 0U;
        for (i = 1U; i < n; ++i) {
            if ((ecg[i] > thresh) && (ecg[i - 1U] <= thresh)) {
                if ((last != 0U) && ((i - last) < 25U)) {
                    continue; /* inside the refractory: noise, not a beat */
                }
                if (np > 0U) {
                    intervals[ni++] = (uint32_t) i - peaks[np - 1U];
                }
                peaks[np++] = (uint32_t) i;
                last = i;
                if (np >= 240U) { break; }
            }
        }
    }
    if (ni < 2U) {
        return 0U; /* two intervals = three beats, the minimum to median */
    }

    /* Median by insertion sort; ni <= 239. */
    for (i = 1U; i < ni; ++i) {
        const uint32_t key = intervals[i];
        size_t j = i;
        while ((j > 0U) && (intervals[j - 1U] > key)) {
            intervals[j] = intervals[j - 1U];
            --j;
        }
        intervals[j] = key;
    }
    median = intervals[ni / 2U];
    return (uint16_t) ((60.0f * BP_FS_HZ) / (float) median + 0.5f);
}

bool bp_plausible(double sbp, double dia, uint16_t hr_pipe, uint16_t hr_session,
                  float ecg_min, float ecg_max)
{
    /* Same bounds the STM32 enforces on the write block -- one clamp, two
     * ends of the link (tb_bp_pair_valid() in tb_regs.h). */
    if (!tb_bp_pair_valid((uint16_t) sbp, (uint16_t) dia)) {
        return false;
    }
    if ((ecg_max - ecg_min) < BP_ECG_FLAT_FLOOR) {
        return false;
    }
    if (hr_session != 0U) {
        /* +/-25% agreement with the session HR, the same trust window
         * Dsp_PickRate applies between ECG and PPG on the STM32. */
        const uint32_t hi = hr_session + (hr_session / 4U);
        const uint32_t lo = hr_session - (hr_session / 4U);
        if ((hr_pipe < lo) || (hr_pipe > hi)) {
            return false;
        }
    }
    return true;
}

/*--------------------------- capture + task --------------------------------*/

void bp_capture_arm_dump(void)
{
    if (s_capturing) {
        ESP_LOGW(TAG, "bplog refused: a measurement is running -- arm it "
                      "before the next one starts");
        return;
    }
    s_dump_armed = true;
}

void bp_capture_record(uint32_t seconds)
{
    uint32_t want;

    if (s_capturing) {
        /* Everything below runs bp_capture_start() from the console task:
         * zeroing s_count and clearing s_capturing mid-window throws away the
         * patient's BP with no message, or truncates it at the record target.
         * The refusal is the whole fix -- the shared state stays lock-free
         * (ownership map in bp_capture.h). */
        ESP_LOGW(TAG, "bplog %u refused: a measurement is running",
                 (unsigned) seconds);
        return;
    }

    want = seconds * (uint32_t) BP_FS_HZ;
    if (want == 0U || want > BP_WINDOW_SAMPLES) {
        want = BP_WINDOW_SAMPLES;
    }
    s_record_target = want;
    s_dump_armed = true;
    s_record_only = true;
    bp_capture_start();
    ESP_LOGI(TAG, "recording %u samples (~%us) -- no measurement needed",
             (unsigned) want, (unsigned) (want / (uint32_t) BP_FS_HZ));
}

bool bp_capture_capturing(void)
{
    return s_capturing;
}

void bp_capture_wave_push(uint32_t ir, uint32_t red, uint16_t ecg)
{
    if (!s_capturing || (s_count >= BP_WINDOW_SAMPLES)) {
        return;
    }
    /* Back to the wire form (see the s_acc comment): counts >> TB_PPG_SHIFT is
     * exactly what tb_ppg_pack() rounded on the far side, so this loses nothing
     * and cannot wrap. ecg is a 12-bit ADC word and needs neither. */
    s_acc[0][s_count] = (uint16_t)(ir >> TB_PPG_SHIFT);
    s_acc[1][s_count] = (uint16_t)(red >> TB_PPG_SHIFT);
    s_acc[2][s_count] = ecg;
    ++s_count;

    /* Record mode has no measure-done to close it, so it closes itself. */
    if ((s_record_target != 0U) && (s_count >= s_record_target)) {
        bp_capture_measure_done();
    }
}

void bp_capture_start(void)
{
    s_count = 0;
    s_capturing = true;
    memset(s_state_red, 0, sizeof(s_state_red));
    memset(s_state_ir, 0, sizeof(s_state_ir));
    memset(s_state_ecg, 0, sizeof(s_state_ecg));
}

void bp_capture_measure_done(void)
{
    s_capturing = false;
    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}

static void bp_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Whatever the window actually collected. s_capturing went false in
         * bp_capture_measure_done() before the notify, so no push can move this
         * while the pass below runs. */
        const uint32_t n = s_count;
        const bool record_only = s_record_only;

        /* Record mode does not need a scoreable window or an honest triage --
         * but it must reset for the next real measurement. */
        s_record_only = false;
        s_record_target = 0U;

        /* Too short to score: the measure was aborted, or a wave gap restarted
         * the capture near the end. The triage model's imputation covers the
         * patient; publishing nothing is honest. */
        if (!record_only && n < BP_MIN_SAMPLES) {
            ESP_LOGW(TAG, "capture short: %u of %u -- BP not computed",
                     (unsigned) n, (unsigned) BP_MIN_SAMPLES);
            tb_ui_source_on_bp(false, 0U, 0U);
            continue;
        }

        {
            /* Doubles for the model: 3 x 48 KB, malloc -> PSRAM (the S3 FPU
             * is single-precision; the model's double math is unavoidable,
             * so it runs here, off the LVGL task, exactly once.) Sized by n,
             * not the window constant: n is what was captured. */
            double *red = malloc(n * sizeof(double));
            double *ir = malloc(n * sizeof(double));
            double *ecg = malloc(n * sizeof(double));
            float *ecg_f = malloc(n * sizeof(float));
            float ecg_min = 0.0f, ecg_max = 0.0f;

            if ((red == NULL) || (ir == NULL) || (ecg == NULL) ||
                (ecg_f == NULL)) {
                ESP_LOGE(TAG, "no memory for the BP window");
                free(red); free(ir); free(ecg); free(ecg_f);
                tb_ui_source_on_bp(false, 0U, 0U);
                continue;
            }

            /*
             * Prime each filter on its own first sample -- see
             * bp_biquad_prime(). Done here rather than in bp_capture_start()
             * because that runs before any sample exists, and the DC level to
             * prime on is the first sample of THIS window.
             */
            bp_biquad_prime((float) tb_ppg_unpack(s_acc[1][0]), bp_coeff_hp05,
                            s_state_red);
            bp_biquad_prime((float) tb_ppg_unpack(s_acc[0][0]), bp_coeff_hp05,
                            s_state_ir);
            bp_biquad_prime((float)(int16_t) s_acc[2][0], bp_coeff_ecg_bp,
                            s_state_ecg);

            for (uint32_t i = 0U; i < n; ++i) {
                const float f_red =
                    bp_biquad_f32((float) tb_ppg_unpack(s_acc[1][i]),
                                  bp_coeff_hp05, s_state_red);
                const float f_ir =
                    bp_biquad_f32((float) tb_ppg_unpack(s_acc[0][i]),
                                  bp_coeff_hp05, s_state_ir);
                /*
                 * The ECG word is SIGNED 16-bit, not a raw 12-bit ADC count:
                 * the AD8232 output is bipolar around its reference and the
                 * word on the wire carries the full signed range. Proven from
                 * the numbers, not assumed -- a stable 5-15 Hz biquad bounds a
                 * 0..4095 input to a filtered span of ~5300, and the hardware
                 * measured 77918 (2026-09-04), which only the 65536-jump of a
                 * wrap can produce. Reading it unsigned made every
                 * zero-crossing a 65536 step: hr_pipe fell to 0 (wrap spikes
                 * sit inside the 250 ms refractory), bp_extract_features
                 * failed, and BP vanished. The (int16_t) cast this line used
                 * to carry implicitly was load-bearing.
                 */
                const float f_ecg =
                    bp_biquad_f32((float)(int16_t) s_acc[2][i],
                                  bp_coeff_ecg_bp, s_state_ecg);

                red[i] = (double) f_red;
                ir[i] = (double) f_ir;
                ecg[i] = (double) f_ecg;
                ecg_f[i] = f_ecg;
                if ((i == 0U) || (f_ecg < ecg_min)) { ecg_min = f_ecg; }
                if ((i == 0U) || (f_ecg > ecg_max)) { ecg_max = f_ecg; }
            }

            double sbp = 0.0, dia = 0.0;
            /* U counts as not-male: the model's training sex feature is
             * binary and unknown is not a third value it has seen. */
            const double is_male =
                (ui_session_get_gender() == UI_GENDER_M) ? 1.0 : 0.0;
            bool ok = bp_predict(red, ir, ecg, n, is_male, &sbp, &dia);

            const uint16_t hr_pipe = bp_hr_from_ecg(ecg_f, n);
            vitals_t v;
            ui_mock_get_vitals(&v);
            const uint16_t hr_session =
                ((v.valid_mask & UI_VITAL_HR) != 0U) ? v.hr : 0U;

            ok = ok && bp_plausible(sbp, dia, hr_pipe, hr_session,
                                    ecg_min, ecg_max);

            /*
             * NO ELECTRODES, NO BLOOD PRESSURE. Checked after the model runs so
             * the CSV dump below still captures the window and the number it
             * would have published -- but the result is not shown, not sent, and
             * not fed to the triage.
             *
             * This is the gate the flat-ECG floor in bp_plausible() was supposed
             * to be and is not. Observed 2026-09-04: two runs with only the
             * finger clip on, one published 113/59 and the other did not, from
             * the same absent electrodes. An open AD8232 input is not flat -- it
             * carries mains hum and cable movement, which clears
             * BP_ECG_FLAT_FLOOR and then reads as R peaks.
             *
             * The features are pulse ARRIVAL TIMES from the ECG R wave to the
             * finger pulse, so without a reference instant there is nothing for
             * the model to measure and its output is a number without a source.
             * The triage's 129.7 mmHg imputation is the honest answer instead.
             *
             * The STM32's own flag is the test, not an ESP32-side amplitude
             * heuristic: it clears TB_FLAG_HR_FROM_PPG only when its 497.5 Hz DSP
             * actually got a rate out of the ECG, which noise does not survive.
             */
            const bool ecg_present = tb_ui_source_ecg_rate_seen();

            if (ok && !ecg_present && !record_only) {
                ESP_LOGW(TAG, "BP %.0f/%.0f discarded: no ECG rate during the "
                              "window -- the pulse transit features have no "
                              "reference, so this number has no source",
                         sbp, dia);
                ok = false;
            }

            /*
             * PUBLISH FIRST, print second. The dump below is ~6000 printf rows
             * (~290 KB) and costs 20-60 s on the USB-Serial-JTAG console; when it
             * ran before this block, `bplog` + a real measurement showed its
             * result that much later than the window closed. Measured 2026-09-05:
             * that WAS the reported "pengukuran selesai agak lag". The verdict is
             * computed above either way, so this reorder costs nothing.
             */
            if (!record_only) {
                if (ok) {
                    ESP_LOGI(TAG, "BP %.0f/%.0f (%u samples, pipe hr %u, "
                                  "session hr %u)", sbp, dia, (unsigned) n,
                             (unsigned) hr_pipe, (unsigned) hr_session);
                    tb_ui_source_on_bp(true, (uint16_t) sbp, (uint16_t) dia);
                } else if (ecg_present) {
                    ESP_LOGW(TAG, "BP implausible: sbp=%.1f dia=%.1f n=%u "
                                  "pipe_hr=%u session_hr=%u ecg_span=%.0f",
                             sbp, dia, (unsigned) n, (unsigned) hr_pipe,
                             (unsigned) hr_session,
                             (double) (ecg_max - ecg_min));
                    tb_ui_source_on_bp(false, 0U, 0U);
                } else {
                    /* The no-ECG branch already said why; a second warning line
                     * about the same discarded reading would just bury it. */
                    tb_ui_source_on_bp(false, 0U, 0U);
                }
            }

            /* ecg_f is the filtered ECG the score above was computed from;
             * s_acc[*][i] is the raw window, unpacked back to counts.
             *
             * The `#` lines make the file self-describing -- pandas takes them
             * with comment='#' -- so a CSV found later still says which
             * coefficients and which prediction produced it. Uniform column
             * count on every data row for the same reason.
             *
             * ponytail: nothing stops the LVGL task starting another measurement
             * mid-print, which would rewrite s_acc under this loop and garble the
             * tail (bounded by n, so no overrun). Wait for the print to finish
             * before scanning the next patient; add a flag if that ever bites. */
            if (s_dump_armed) {
                s_dump_armed = false;
                printf("# triagebox bp window: n=%u fs=%.1f sbp=%.1f dia=%.1f "
                       "published=%d ecg_present=%d pipe_hr=%u session_hr=%u "
                       "is_male=%.0f\n",
                       (unsigned) n, BP_FS_HZ, sbp, dia, (int) ok,
                       (int) ecg_present, (unsigned) hr_pipe,
                       (unsigned) hr_session, is_male);
                printf("# hp05={%.6f,%.6f,%.6f,%.6f,%.6f} "
                       "ecg_bp={%.6f,%.6f,%.6f,%.6f,%.6f} (b0,b1,b2,-a1,-a2)\n",
                       bp_coeff_hp05[0], bp_coeff_hp05[1], bp_coeff_hp05[2],
                       bp_coeff_hp05[3], bp_coeff_hp05[4],
                       bp_coeff_ecg_bp[0], bp_coeff_ecg_bp[1],
                       bp_coeff_ecg_bp[2], bp_coeff_ecg_bp[3],
                       bp_coeff_ecg_bp[4]);
                printf("i,raw_ir,raw_red,raw_ecg,fil_ir,fil_red,fil_ecg\n");
                for (uint32_t i = 0U; i < n; ++i) {
                    printf("%u,%u,%u,%d,%.4f,%.4f,%.4f\n", (unsigned) i,
                           (unsigned) tb_ppg_unpack(s_acc[0][i]),
                           (unsigned) tb_ppg_unpack(s_acc[1][i]),
                           (int)(int16_t) s_acc[2][i], ir[i], red[i],
                           (double) ecg_f[i]);
                }
                printf("# end (%u rows)\n", (unsigned) n);
                fflush(stdout);
            }

            free(red); free(ir); free(ecg); free(ecg_f);
        }
    }
}

esp_err_t bp_capture_init(void)
{
    if (xTaskCreate(bp_task, "bp_predict", 16384, NULL, 2, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "BP task not created -- BP will never publish and every "
                      "measurement waits out its full window for nothing");
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    /* The measure window is what a patient waits through, so log that rather
     * than the accumulator's capacity -- they differ now (45 s vs 60 s). */
    ESP_LOGI(TAG, "BP capture up: %ums window, >=%u samples to score, "
                  "task ready",
             (unsigned) UI_MEASURE_MS, (unsigned) BP_MIN_SAMPLES);
    return ESP_OK;
}
