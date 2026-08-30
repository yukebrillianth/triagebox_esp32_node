#ifndef UI_LOGIC_UI_RUNTIME_H
#define UI_LOGIC_UI_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Wires ui_mock + ui_session + ui_nav into one host-drivable loop.
 * No LVGL and no generated screens required: screens read ui_session.
 * Later (Task 26+) the same tick is called from the LVGL timer; screen
 * setters run under lvgl_port_lock on device, no lock in the sim loop.
 */

void ui_runtime_init(void);

/* Call from LVGL timer / sim loop with monotonic ms. Returns immediately. */
void ui_runtime_tick(uint32_t now_ms);

/* QA shortcut: cycle mock priority GREEN→YELLOW→RED→BLACK (bind to a sim key).
 * On Monitor it also takes the deterioration path when the new colour is worse,
 * which is the only way to exercise the alarm on a desktop -- the mock's priority
 * never changes on its own. */
void ui_runtime_debug_cycle_priority(void);

/*
 * Monitor re-triage period in ms, 0 = off. Set from the Menu; a plain static, so
 * it goes back to UI_RETRIAGE_DEFAULT_MS on every boot -- there is no settings
 * store in this tree (see ui_demo.h for the same trade, made for the same
 * reason), and the default is the safe end of the choice anyway.
 */
#define UI_RETRIAGE_DEFAULT_MS 30000U

void ui_runtime_set_retriage_ms(uint32_t ms);
uint32_t ui_runtime_retriage_ms(void);

/*
 * True once when the last re-triage came out MORE severe than the priority the
 * session was carrying. ui_bindings consumes it to sound the alarm pattern and
 * blink the Result banner; ui_runtime has already navigated to Result and updated
 * the session by then.
 *
 * Same take-once shape as ui_action_take_*_request(), and for the same reason:
 * this file stays LVGL-free.
 */
bool ui_runtime_take_degraded(void);

#endif /* UI_LOGIC_UI_RUNTIME_H */
