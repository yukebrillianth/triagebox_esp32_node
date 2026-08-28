#ifndef UI_LOGIC_UI_DEMO_H
#define UI_LOGIC_UI_DEMO_H

#include "ui_types.h"

/*
 * Demo mode: fabricated vitals and a fixed RED result, for filming.
 *
 * WHAT IS FAKE: the four vital readings, the per-sensor health mask, and the
 * triage result. WHAT STAYS REAL: the RFID tag, the buttons, the battery, the
 * link to the STM32, and the RESULT frame sent back to it -- so the station and
 * the dashboard see the demo patient exactly as they would see a real one.
 *
 * SAFETY: off at every boot, on purpose -- there is no NVS write here. Someone
 * who forgets to switch it off after filming gets a normal device on the next
 * power cycle, instead of a box that calls every patient RED. That is also why
 * this is not persisted "for convenience": the convenience is the hazard.
 *
 * Deliberately NOT shown as an on-screen badge, because the point is footage
 * that looks like the real thing. The trade is the boot reset above, plus a
 * warning in the log on every inference.
 *
 * The flag lives here rather than in ui_mock.h because that header has two
 * implementations and this needs exactly one. In the simulator the mock feed is
 * already synthetic, so the toggle changes nothing visible there; it only bites
 * on device, where the feed is real.
 */

bool ui_demo_enabled(void);
void ui_demo_set(bool on);
void ui_demo_toggle(void);

/*
 * The result demo mode reports. RED with a high-but-not-suspicious confidence:
 * 1.00 would be the one number a real classifier never prints.
 */
#define UI_DEMO_PRIORITY   UI_PRIORITY_RED
#define UI_DEMO_CONFIDENCE 0.93f

/*
 * Overwrite the four readings with a patient whose numbers justify RED -- HR
 * ~128, SpO2 ~88, RR ~32, BP ~86/54. Made to agree with the verdict on purpose:
 * a clip showing MERAH next to HR 72 and SpO2 99 reads as a broken device.
 *
 * The values drift slowly (a fixed wobble on a ~0.8 s step, so it is
 * reproducible frame to frame) because four numbers frozen for the whole take
 * look like a screenshot, not a measurement.
 *
 * out->battery is left alone: the real gauge reading is still the true one, and
 * nothing about a fake battery helps the video.
 */
void ui_demo_vitals(uint32_t now_ms, vitals_t *out);

/*
 * Sensor mask to report while demo mode is on: everything up. Without this the
 * Home dot reads "Sensor 1/4" next to four live-looking numbers, which is the
 * one frame of footage that gives the whole thing away.
 */
uint8_t ui_demo_sensor_mask(void);

#endif /* UI_LOGIC_UI_DEMO_H */
