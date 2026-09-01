#ifndef BP_CAPTURE_H
#define BP_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Cuff-less BP: accumulate the STM32's 3-channel waveform during the 60 s
 * measurement, run the LightGBM pair once at measure-done, and publish the
 * result through tb_ui_source_on_bp().
 *
 * Ownership map: the poll task (tb_link_i2c.c) is the only caller of
 * bp_capture_wave_push(); ui_mock_tick()'s measure-done branch (LVGL task) is
 * the only caller of bp_capture_start()/bp_capture_measure_done(); the BP task
 * created here is the only one that runs bp_predict(). Every cross-task edge
 * goes through FreeRTOS primitives or tb_ui_source's spinlock -- no naked
 * shared state.
 */

/* Called after tb_link_start() from app_main: creates the BP task. */
void bp_capture_init(void);

/* LVGL task, from ui_mock_tick: ui_mock_start_measure()'s counterpart. */
void bp_capture_start(void);

/* LVGL task, measure-done branch: freeze the window, wake the BP task. */
void bp_capture_measure_done(void);

/* Poll task: one sample from the STM32's ring. ir/red in MAX30102 counts
 * (already tb_ppg_unpack()-ed by the caller), ecg in raw 12-bit ADC counts. */
void bp_capture_wave_push(uint32_t ir, uint32_t red, uint16_t ecg);

/* Poll task asks before each push (the capture runs only during a measure). */
bool bp_capture_capturing(void);

/*---- Pure helpers, host-tested (bp_capture_selftest.c), no FreeRTOS ---- */

/*
 * One direct-form-I biquad pass. Coefficients b0,b1,b2,-a1,-a2 (the CMSIS
 * sign convention this repo already uses in dsp code on the STM32 side);
 * s[0]/s[1] are the two state words, per channel -- keep them separate per
 * signal.
 */
float bp_biquad_f32(float x, const float coeffs[5], float state[2]);

/* Simple above-0.6*max peak picker with a 250 ms refractory; median of the
 * intervals -> bpm, 0 when fewer than three peaks (not enough to median). */
uint16_t bp_hr_from_ecg(const float *ecg, size_t n);

/*
 * The publish gate: ranges identical to the STM32's tb_bp_pair_valid() (which
 * guards the same numbers one link away), plus a flat-ECG floor. hr_session 0
 * means "no session HR" and skips the agreement check.
 */
bool bp_plausible(double sbp, double dia, uint16_t hr_pipe, uint16_t hr_session,
                  float ecg_min, float ecg_max);

#endif /* BP_CAPTURE_H */
