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
 * QA shortcut: real hardware measure window is 60000 ms.
 * Keep this <= 5000 so host/sim UI can exercise Mengukur → Result quickly.
 */
#ifndef UI_MEASURE_MS
#define UI_MEASURE_MS 2000U
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

/* GREEN → YELLOW → RED → BLACK → GREEN … */
void ui_mock_cycle_priority(void);

/* Single-slot button buffer for keypad indev / host tests. */
void ui_mock_push_button(uint8_t index, bool pressed);
bool ui_mock_pop_button(btn_event_t *out);

/* Health of the STM32 link, for the Home status dots. */
void ui_mock_get_link_status(link_status_t *out);

/* Operator confirmed power-off: tell the STM32, which owns the power hold. */
void ui_mock_power_off(void);

#endif /* UI_LOGIC_UI_MOCK_H */
