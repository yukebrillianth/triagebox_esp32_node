#ifndef UI_LOGIC_UI_BOARD_H
#define UI_LOGIC_UI_BOARD_H

#include <stdbool.h>
#include <stdint.h>
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

/*
 * Cut the board's own power via the SW6106 PMIC on the shared I2C bus.
 *
 * This is a real shutdown, not a request: on board V3.0 the battery and the
 * 5 V/3V3 rails belong to the SW6106, and its register list exposes the same
 * "output power off" event the physical key triggers. Returns only if the write
 * failed -- on success the rails drop and execution ends here.
 *
 * Verified on hardware: it powers down **even with USB connected**, so there is
 * no way to exercise this without actually losing the board. Persist anything
 * that must survive before calling.
 *
 * Give the STM32 its POWER_OFF frame first so it can park sensors and stop
 * transmitting; this call does not wait for it. No-op in the sim.
 */
void ui_board_power_off(void);

#endif /* UI_LOGIC_UI_BOARD_H */
