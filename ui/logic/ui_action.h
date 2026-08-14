#ifndef UI_LOGIC_UI_ACTION_H
#define UI_LOGIC_UI_ACTION_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_nav.h"

/*
 * Single dispatcher for touch ButtonBar cells and keypad keys.
 * btn_id is 0..3 left-to-right (empty cells are no-ops).
 *
 * LV_KEY mapping (matches ui_input keypad):
 *   PREV→0, NEXT→1, ENTER→2, ESC→3
 * This preserves the physical ButtonBar positions on Age/Gender:
 *   Up→0, Down→1, Back→2, Select→3.
 *
 * Age/Gender Up/Down (btn 0/1): reserved for focus movement — no nav change
 * in the action table until list focus is wired.
 */
void ui_action(ui_screen_id_t screen, uint8_t btn_id);

/* Map LV_KEY_* → btn 0..3 on the current screen, then call ui_action. */
void ui_action_on_key(uint32_t lv_key);

/*
 * Power is confirmed, not immediate: losing a running measurement because the
 * button was brushed would throw away a patient's triage. ui_action only raises
 * the request; ui_bindings shows the dialog and calls ui_mock_power_off() on
 * confirm. Kept as a flag so this file stays LVGL-free.
 */
bool ui_action_take_power_request(void);

/*
 * True once if the last action was a commit-type press worth a beep (Scan,
 * Start, Select, Stop, Reset) rather than list navigation. Raised here, not in
 * the touch callback, so physical buttons and touch beep identically — same
 * reason ui_action() is the single dispatcher.
 */
bool ui_action_take_beep_request(void);

#endif /* UI_LOGIC_UI_ACTION_H */
