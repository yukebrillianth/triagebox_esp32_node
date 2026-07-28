#ifndef UI_LOGIC_UI_RUNTIME_H
#define UI_LOGIC_UI_RUNTIME_H

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

/* QA shortcut: cycle mock priority GREEN→YELLOW→RED→BLACK (bind to a sim key). */
void ui_runtime_debug_cycle_priority(void);

#endif /* UI_LOGIC_UI_RUNTIME_H */
