#include "bp_capture.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bp_pipeline.h"
#include "tb_regs.h"
#include "tb_ui_source.h"
#include "ui_mock.h"
#include "ui_session.h"
#include "ui_types.h"

static const char *TAG = "bp_capture";

/* The model was trained on 60 s @ 100 Hz (sample_signals.h: 6000 samples).
 * UI_MEASURE_MS is also 60 s, so the accumulator fills exactly once per
 * measurement; a longer one stops appending rather than wrapping. */
#define BP_WINDOW_SAMPLES 6000U
#define BP_FS_HZ 100.0f

/* 3 channels x 6000 x 2 B = 36 KB of internal BSS. PSRAM is not used here on
 * purpose: the push site is the poll task at 20 Hz writing three stores, and
 * the read side runs once per measurement. Internal RAM has it. */
static int16_t s_acc[3][BP_WINDOW_SAMPLES];
static uint32_t s_count; /* samples appended (all three channels together) */
static bool s_capturing;

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
 * ECG: bandpass 5-15 Hz, Q 0.75, for R-peak TIMING only. The STM32's own DSP
 * runs the same band at 497.5 Hz for its bpm work; this is the same passband
 * at the wire's 100 Hz so the peak timing agrees.
 */
static const float k_hp05[5] = {0.978027f, -1.956055f, 0.978027f,
                                1.955572f, -0.956537f};
static const float k_ecg_bp[5] = {0.230114f, 0.0f, -0.230114f,
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
    /* coeffs = {b0, b1, b2, -a1, -a2}; state carries x[n-1], x[n-2] and the
     * previous output folded in (direct form I rearranged to two words). */
    const float y = coeffs[0] * x + state[0];
    state[0] = coeffs[1] * x - coeffs[3] * y + state[1];
    state[1] = coeffs[2] * x - coeffs[4] * y;
    return y;
}

uint16_t bp_hr_from_ecg(const float *ecg, size_t n)
{
    /* Peaks at most every 250 ms (25 samples): a heart faster than 240 bpm is
     * not a heart this box can measure anyway, and the cap bounds the arrays.
     * 6000/25 = 240 slots, sized off the window constant. */
    uint32_t peaks[240];
    uint32_t intervals[240];
    size_t np = 0, ni = 0, i;
    float max = ecg[0], min = ecg[0];
    uint32_t median;

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

bool bp_capture_capturing(void)
{
    return s_capturing;
}

void bp_capture_wave_push(uint32_t ir, uint32_t red, uint16_t ecg)
{
    if (!s_capturing || (s_count >= BP_WINDOW_SAMPLES)) {
        return;
    }
    s_acc[0][s_count] = (int16_t) ir;
    s_acc[1][s_count] = (int16_t) red;
    s_acc[2][s_count] = ecg;
    ++s_count;
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

        /* Short window: the measure ended early (abort, screen change). The
         * triage model's imputation covers the patient; publishing nothing is
         * honest. */
        if (s_count < BP_WINDOW_SAMPLES) {
            ESP_LOGW(TAG, "capture short: %u of %u -- BP not computed",
                     (unsigned) s_count, (unsigned) BP_WINDOW_SAMPLES);
            tb_ui_source_on_bp(false, 0U, 0U);
            continue;
        }

        {
            /* Doubles for the model: 3 x 48 KB, malloc -> PSRAM (the S3 FPU
             * is single-precision; the model's double math is unavoidable,
             * so it runs here, off the LVGL task, exactly once.) */
            double *red = malloc(BP_WINDOW_SAMPLES * sizeof(double));
            double *ir = malloc(BP_WINDOW_SAMPLES * sizeof(double));
            double *ecg = malloc(BP_WINDOW_SAMPLES * sizeof(double));
            float *ecg_f = malloc(BP_WINDOW_SAMPLES * sizeof(float));
            float ecg_min = 0.0f, ecg_max = 0.0f;

            if ((red == NULL) || (ir == NULL) || (ecg == NULL) ||
                (ecg_f == NULL)) {
                ESP_LOGE(TAG, "no memory for the BP window");
                free(red); free(ir); free(ecg); free(ecg_f);
                tb_ui_source_on_bp(false, 0U, 0U);
                continue;
            }

            for (uint32_t i = 0U; i < BP_WINDOW_SAMPLES; ++i) {
                const float f_red = bp_biquad_f32((float) s_acc[1][i],
                                                  k_hp05, s_state_red);
                const float f_ir = bp_biquad_f32((float) s_acc[0][i],
                                                 k_hp05, s_state_ir);
                const float f_ecg = bp_biquad_f32((float) s_acc[2][i],
                                                  k_ecg_bp, s_state_ecg);

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
            bool ok = bp_predict(red, ir, ecg, BP_WINDOW_SAMPLES, is_male,
                                 &sbp, &dia);

            const uint16_t hr_pipe = bp_hr_from_ecg(ecg_f,
                                                    BP_WINDOW_SAMPLES);
            vitals_t v;
            ui_mock_get_vitals(&v);
            const uint16_t hr_session =
                ((v.valid_mask & UI_VITAL_HR) != 0U) ? v.hr : 0U;

            ok = ok && bp_plausible(sbp, dia, hr_pipe, hr_session,
                                    ecg_min, ecg_max);

            free(red); free(ir); free(ecg); free(ecg_f);

            if (ok) {
                ESP_LOGI(TAG, "BP %.0f/%.0f (pipe hr %u, session hr %u)",
                         sbp, dia, (unsigned) hr_pipe, (unsigned) hr_session);
                tb_ui_source_on_bp(true, (uint16_t) sbp, (uint16_t) dia);
            } else {
                ESP_LOGW(TAG, "BP implausible: sbp=%.1f dia=%.1f pipe_hr=%u "
                              "session_hr=%u ecg_span=%.0f",
                         sbp, dia, (unsigned) hr_pipe, (unsigned) hr_session,
                         (double) (ecg_max - ecg_min));
                tb_ui_source_on_bp(false, 0U, 0U);
            }
        }
    }
}

void bp_capture_init(void)
{
    if (xTaskCreate(bp_task, "bp_predict", 16384, NULL, 2, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "BP task not created");
        s_task = NULL;
        return;
    }
    ESP_LOGI(TAG, "BP capture up: %us window, task ready",
             (unsigned) (BP_WINDOW_SAMPLES / 100U));
}
