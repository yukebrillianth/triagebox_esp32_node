#ifndef UI_LOGIC_UI_BOARD_H
#define UI_LOGIC_UI_BOARD_H

#include <stdbool.h>

/*
 * Board outputs the UI needs to drive. Like ui_mock.h this has two
 * implementations picked in CMake: a no-op in ui/logic/ui_board_stub.c for the
 * sim and Editor preview, and the real one in components/triagebox_board/.
 *
 * Deliberately dumb on/off calls — all sequencing (beep patterns, idle timeout)
 * lives in ui_bindings.c where it is one LVGL timer and stays testable.
 */

/* Bring up the IO expander. No-op in the sim. Call after the display starts
 * (needs I2C) and before the first backlight/buzzer call. */
void ui_board_init(void);

/* Backlight enable. On this board it is BL_EN behind the TCA9554, not a PWM
 * pin, so it is on/off only — there is no dimming to be had. */
void ui_board_backlight(bool on);

/* Active buzzer: it self-oscillates, so this is enable/disable, not a tone. */
void ui_board_buzzer(bool on);

#endif /* UI_LOGIC_UI_BOARD_H */
