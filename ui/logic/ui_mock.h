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
 * Measure window. 60 s is not a round number picked for feel -- respiratory rate
 * is counted from the breathing microphone, and a rate per MINUTE needs a minute
 * of audio before it means anything. Shortening this does not make the box
 * faster, it makes RR a guess extrapolated from a few breaths.
 *
 * This used to default to 2000 with a comment saying hardware was 60000, and
 * nothing set 60000 anywhere -- so the device ran a 2 s window. The default is
 * now the real value, and the fast one is opt-in for desktop QA:
 * sim/CMakeLists.txt and tools/run_selftests.sh both pass -DUI_MEASURE_MS=2000.
 * Keep any override <= 5000 so the host/sim UI still steps through quickly.
 */
#ifndef UI_MEASURE_MS
#define UI_MEASURE_MS 60000U
#endif

#ifndef UI_MOCK_SCAN_MS
#define UI_MOCK_SCAN_MS 500U
#endif

void ui_mock_init(void);

/* Drive all timed mock state. Caller (LVGL timer) supplies monotonic now_ms. */
void ui_mock_tick(uint32_t now_ms);

/* RFID: start scan → after UI_MOCK_SCAN_MS, tag "3021" is ready once. */
void ui_mock_start_scan(void);
bool ui_mock_rfid_ready(rfid_t *out);

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
 * Operator confirmed power-off. On device this tells the STM32 (so it can park
 * sensors and LoRa), then cuts the rail via the SW6106 PMIC -- it does not
 * return. The sim implementation just logs.
 */
void ui_mock_power_off(void);

#endif /* UI_LOGIC_UI_MOCK_H */
