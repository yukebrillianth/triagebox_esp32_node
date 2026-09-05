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
 * Battery state from the SW6106's own fuel gauge. Returns false when the PMIC
 * could not be read, and then leaves *percent and *charging untouched -- the
 * caller shows "--%" rather than a fabricated 0%.
 *
 * The gauge lives on the ESP32's I2C bus, not on the STM32, so this does not go
 * through the link: TB_REG_BATTERY is whatever the STM32 measures, and the STM32
 * is not connected to the pack.
 *
 * Read-only by design. The SW6106 is a 4 A charger with a LiPo on it, and its
 * write-unlock registers (0x01, 0x22) can latch the power path off; the only
 * write in this tree is ui_board_power_off(). No-op returning false in the sim.
 */
bool ui_board_battery(uint8_t *percent, bool *charging);

/*
 * Pack voltage in millivolts, or false when the PMIC could not be read (and then
 * *millivolts is left untouched, same contract as above).
 *
 * Separate call rather than a third output on ui_board_battery(): it costs two
 * more I2C reads on the bus the touch controller shares, and only the range-test
 * screen wants it. The status bar wants a percentage, and for that the gauge's own
 * SoC is the better number -- deriving one from this voltage would need a
 * discharge curve for a pack nobody has characterised.
 *
 * Worth showing on a diagnostics screen anyway: a fuel gauge is an estimate, and
 * a pack sagging under LoRa TX shows up here before the percentage moves.
 *
 * Vbat = ((0x15[3:0] << 8) | 0x14) * 1.2 mV, from the SW6106 I2C Register List
 * RG006_1_v1.2 §2.15 -- the same decode the `pmic` console command prints.
 */
bool ui_board_battery_mv(uint16_t *millivolts);

/*
 * Battery-side discharge current in milliamps, or false when the PMIC could not
 * be read (*milliamps then untouched, same contract as above).
 *
 * BATTERY SIDE, not the 5 V rail: this is what the pack is delivering to
 * everything the board powers -- ESP32, panel, backlight, STM32 and the sensors
 * hanging off it -- so it is the number that predicts runtime from a mAh rating.
 * It is NOT the same as Vbat * this / 5 V; the boost converter's losses are not
 * in it.
 *
 * ZERO WHILE CHARGING, and that is the part to know before putting it on a
 * screen. Ichg (0x18[3:0]/0x1A) and Idischg are mutually exclusive registers on
 * the SW6106: with USB plugged in the pack is being charged, so the discharge
 * counter reads 0 and the display will honestly say 0 mA. Unplug to measure
 * draw. ui_board_battery()'s `charging` flag is how a caller can say which.
 *
 * Idischg = (((0x18[7:4] << 8) | 0x19) * 25 / 7) mA, from the SW6106 I2C Register
 * List RG006_1_v1.2 -- the same decode the `pmic` console command prints.
 */
bool ui_board_battery_ma(uint16_t *milliamps);

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
 * Call this BEFORE telling the STM32 to park -- seven internal paths can return
 * without cutting anything, and a sensor board already told to go quiet beside a
 * box that stayed on is a node that looks alive and reports nothing. The caller
 * (tb_ui_source.c's ui_mock_power_off) sends POWER_OFF only if this returns.
 * No-op in the sim.
 */
void ui_board_power_off(void);

#endif /* UI_LOGIC_UI_BOARD_H */
