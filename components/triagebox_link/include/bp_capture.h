#ifndef BP_CAPTURE_H
#define BP_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/*
 * Cuff-less BP: accumulate the STM32's 3-channel waveform during the
 * UI_MEASURE_MS measurement window, run the LightGBM pair once at measure-done,
 * and publish the result through tb_ui_source_on_bp().
 *
 * Ownership map -- who calls what, and from which task:
 *   poll task (tb_link_i2c.c)  bp_capture_wave_push(), bp_capture_capturing(),
 *                              bp_capture_start() on a wave gap, and
 *                              bp_capture_measure_done() through the record
 *                              window's self-close inside wave_push();
 *   LVGL task (tb_ui_source.c) bp_capture_start() at measure start,
 *                              bp_capture_measure_done() at measure done;
 *   debug console (tb_debug.c) bp_capture_arm_dump()/bp_capture_record(),
 *                              which call bp_capture_start() -- refused while
 *                              a measurement is running;
 *   the BP task created here   everything else: reads s_acc, runs
 *                              bp_predict(), prints the dump, publishes.
 * The shared statics (s_count/s_capturing/s_acc) are plain words touched by
 * three tasks with no primitive around them; the arm-refusal plus the
 * s_capturing gate in wave_push are the only thing keeping a `bplog` from
 * destroying a patient's window.
 * ponytail: no lock -- a `bplog` in the gap between measure_done() and the BP
 * task's read of s_count could still reset the accumulator under it. Accepted
 * because a console command fighting a live measurement is operator error; a
 * critical section around start/measure_done/wave_push is the upgrade if that
 * ever stops being true.
 */

/* Called after tb_link_start() from app_main: creates the BP task. Returns
 * ESP_ERR_NO_MEM when the task could not be created -- without it BP never
 * publishes and every measurement waits out its full window for nothing. */
esp_err_t bp_capture_init(void);

/* LVGL task, from ui_mock_tick: ui_mock_start_measure()'s counterpart. */
void bp_capture_start(void);

/* LVGL task, measure-done branch: freeze the window, wake the BP task. */
void bp_capture_measure_done(void);

/* Poll task: one sample from the STM32's ring. ir/red in MAX30102 counts
 * (already tb_ppg_unpack()-ed by the caller), ecg in raw 12-bit ADC counts. */
void bp_capture_wave_push(uint32_t ir, uint32_t red, uint16_t ecg);

/* Poll task asks before each push (the capture runs only during a measure). */
bool bp_capture_capturing(void);

/*
 * Arm a one-shot CSV dump of the NEXT completed window (raw + filtered, all
 * three channels, plus the prediction in a `#` header the file carries with
 * it). Debug-console only (`bplog`); the dump prints after the verdict, from
 * the BP task, so it never delays one. Exists to compare the shipped RBJ
 * biquads against a candidate filter offline, on real signals.
 */
void bp_capture_arm_dump(void);

/*
 * Record mode (`bplog <seconds>`): capture a waveform window with NO patient,
 * NO measurement and NO screen flow, and dump it as the same CSV. For signal
 * analysis -- the publish gates (30 s floor, ECG-present) are skipped because
 * there is no triage to be honest to, and nothing is sent to the STM32. The
 * STM32 pushes its ring unconditionally, so this works on a bare board with
 * just the sensors attached.
 */
void bp_capture_record(uint32_t seconds);

/*---- Pure helpers, host-tested (bp_capture_selftest.c), no FreeRTOS ---- */

/*
 * One direct-form-II-transposed biquad pass. Coefficients b0,b1,b2,-a1,-a2 --
 * the CMSIS convention this repo already uses in the STM32's dsp code, in which
 * the two feedback coefficients are stored ALREADY NEGATED and are therefore
 * added, not subtracted. s[0]/s[1] are the two state words, per channel -- keep
 * them separate per signal.
 */
float bp_biquad_f32(float x, const float coeffs[5], float state[2]);

/*
 * Put `state` in the DC steady state for a constant input x0, so the first real
 * sample does not look like a step from zero. Both coefficient sets here are
 * zero-at-DC, which is what makes this exact rather than approximate -- see the
 * measurement in the implementation.
 */
void bp_biquad_prime(float x0, const float coeffs[5], float state[2]);

/*
 * The two filters the capture actually runs, exposed so the host selftest can
 * check THESE arrays rather than a locally re-derived copy. That distinction is
 * not academic: the test used to derive its own coefficients in the opposite
 * sign convention, which matched the (then wrong) implementation and so passed
 * while the shipped filter diverged to infinity on every measurement.
 *
 * hp05:   high-pass 0.5 Hz, Q 0.7071, for both PPG channels.
 * ecg_bp: band-pass 5-15 Hz (f0 8.6603, Q 0.866), ECG R-peak timing only.
 */
extern const float bp_coeff_hp05[5];
extern const float bp_coeff_ecg_bp[5];

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
