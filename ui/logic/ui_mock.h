#ifndef UI_LOGIC_UI_MOCK_H
#define UI_LOGIC_UI_MOCK_H

#include "ui_types.h"

/*
 * This header has TWO implementations, chosen in CMake — not by #ifdef:
 *   sim/    -> ui/logic/ui_mock.c                      (deterministic fake)
 *   main/   -> components/triagebox_link/tb_ui_source.c (RS485 + SVM)
 * Anything added here must be implemented in both.
 *
 * ponytail: the name says "mock" but it is really the data-feed seam; renaming
 * to ui_feed.h touches 6 files plus the handoff docs already sent out, so it
 * waits until one of those needs editing anyway.
 */

/*
 * Measure window. 45 s, measured rather than chosen for feel.
 *
 * It was 60 s, justified by respiratory rate needing a minute of breathing
 * microphone. That microphone does not exist -- RR is typed in on its own screen
 * (ui_rr.c) -- so the only thing the length still serves is the BP model, whose
 * training windows are 60 s (PKM_BP deploy/extracted_features_60s.csv).
 *
 * What 45 s costs, measured on bp_window.csv with the real feature extractor and
 * both shipped LightGBM models (tools/bp_window_sweep.c, prefixes of one 57.8 s
 * capture, filters primed as the firmware now primes them):
 *
 *     60 s  106.6 / 70.9   (reference)
 *     50 s  106.8 / 66.2   +0.3 / -4.6 mmHg
 *     45 s  108.1 / 66.5   +1.5 / -4.4 mmHg
 *     30 s  109.9 / 66.1   +3.4 / -4.7 mmHg
 *
 * SBP moves 1.5 mmHg and DBP 4.4, against a device that reads ~27 mmHg low
 * versus a cuff to begin with, and against a chain where re-sampling the golden
 * vector onto a half-millisecond-shifted grid moves SBP 14 mmHg. So the window
 * length is not what limits this number.
 *
 * ONE RECORDING, ONE SUBJECT. If a paired cuff comparison ever disagrees, put
 * this back to 60000 -- one constant; BP_MIN_SAMPLES stays 3000, it is gap
 * tolerance rather than a window target (see bp_capture.c). Do not go below
 * ~30 s, the BP_MIN_SAMPLES floor at the measured 98.97 Hz: below it the model
 * can never score and the triage imputes 129.7 mmHg -- bp_capture.c's
 * _Static_asserts fail the build first.
 *
 * Desktop QA overrides it further: sim/CMakeLists.txt and tools/run_selftests.sh
 * both pass -DUI_MEASURE_MS=2000. Keep any override <= 5000 so the host/sim UI
 * still steps through quickly.
 */
/*
 * The hardware window, spelled so a -D cannot displace it: the desktop builds
 * above define UI_MEASURE_MS itself, so UI_MEASURE_MS's #ifndef default never
 * fires there. Code that must reason about the REAL on-device window -- the
 * floor binds in bp_capture.c -- reads this constant, not UI_MEASURE_MS.
 */
#define UI_MEASURE_MS_DEFAULT 45000U
#ifndef UI_MEASURE_MS
#define UI_MEASURE_MS UI_MEASURE_MS_DEFAULT
#endif

#ifndef UI_MOCK_SCAN_MS
#define UI_MOCK_SCAN_MS 500U
#endif

/*
 * How often the Scanning screen re-arms the sensor board's RFID scan.
 *
 * The board's scan is a BOUNDED retry window, not a standing request:
 * RFID_SCAN_WINDOW_MS in the STM32's main.c is 30 s, after which it stops
 * polling the PN532 and says nothing about it. START_SCAN used to be sent once,
 * on the screen-change edge, so an operator who spent more than 30 s finding the
 * patient's card tapped it against a reader that had stopped looking -- silently,
 * with the Scanning screen still inviting them to try. That is one of the two
 * halves of "kadang RFID ga detect"; the other is the gate deadlock in
 * tb_ui_source.c.
 *
 * 25 s keeps the windows overlapping with 5 s of margin for a poll that is late
 * or lost, and costs one register write per 25 s.
 */
#ifndef UI_SCAN_REARM_MS
#define UI_SCAN_REARM_MS 25000U
#endif

void ui_mock_init(void);

/* Drive all timed mock state. Caller (LVGL timer) supplies monotonic now_ms. */
void ui_mock_tick(uint32_t now_ms);

/* RFID: start scan → after UI_MOCK_SCAN_MS, tag "3021" is ready once. */
void ui_mock_start_scan(void);
bool ui_mock_rfid_ready(rfid_t *out);

/*
 * The patient has left this node: forget the tag, the score and the predicted
 * pressure. Called from ui_nav_go() on the way to Home, which is the one place
 * that already means "session over" (it is where ui_session_reset() runs).
 *
 * Not cosmetic. The sensor board keeps its triage result standing on purpose, so
 * a reading taken between scores carries the last verdict rather than dropping to
 * "unscored" -- and the station publishes a vital for as long as a verdict stands.
 * Without this call it stands forever, so the node keeps reporting the patient
 * every 15 s after they are gone, with every measurement blank because nobody is
 * on the sensors. Those empty readings are the newest ones the dashboard has, so
 * they bury the real measurement.
 */
void ui_mock_end_session(void);

/* Measure: start → progress 0..100 over UI_MEASURE_MS → done. */
void ui_mock_start_measure(void);
uint8_t ui_mock_measure_progress(void);
bool ui_mock_measure_done(void);

void ui_mock_get_vitals(vitals_t *out);

/* Hardcoded QA priority — NOT computed from vitals. */
ui_priority_t ui_mock_get_priority(void);
float ui_mock_get_confidence(void);
const char *ui_mock_get_reasons(void);

/*
 * Raw ESI 1..5 behind the colour, or 0 when the model refused to score.
 *
 * Separate from the priority because the colour cannot be un-collapsed: ESI 3, 4
 * and 5 are all GREEN under the three-colour START grouping, so the colour alone
 * cannot tell a walking-wounded patient from a borderline one. Shown on Result
 * under the patient ID, and logged.
 */
int ui_mock_get_esi(void);

/* GREEN → YELLOW → RED → BLACK → GREEN … */
void ui_mock_cycle_priority(void);

/*
 * Re-run classification against the latest snapshot.
 *
 * In the sim: no-op (ui_mock_get_priority() already returns the current cycle
 * colour, and ui_runtime_debug_cycle_priority() advances it).
 * On hardware (tb_ui_source.c): clears the "inference done" latch and runs the
 * SVM against the latest vitals, then transmits the new verdict to the STM32.
 */
void ui_mock_reclassify(void);

/* Single-slot button buffer for keypad indev / host tests. */
void ui_mock_push_button(uint8_t index, bool pressed);
bool ui_mock_pop_button(btn_event_t *out);

/* Health of the STM32 link, for the Home status dots. */
void ui_mock_get_link_status(link_status_t *out);

/*
 * Operator confirmed power-off. On device this cuts the rail via the SW6106
 * PMIC first, and only tells the STM32 to park sensors and LoRa if that write
 * failed -- a box that has been told to go quiet but is still powered reports
 * nothing while looking alive, which is worse than either outcome alone. It
 * does not return when the rail drops. The sim implementation just logs.
 */
void ui_mock_power_off(void);

#endif /* UI_LOGIC_UI_MOCK_H */
