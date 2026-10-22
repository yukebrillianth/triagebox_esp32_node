#ifndef UI_STATUS_H
#define UI_STATUS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Home screen status dots. */
typedef enum {
    UI_STATUS_OK,
    UI_STATUS_WARN,  /* partially degraded — some sensors down */
    UI_STATUS_ERROR  /* no data at all / link down */
} ui_status_state_t;

/*
 * Sensor bitmask exactly as the STM32 publishes it in TB_REG_SENSOR_OK. Bit
 * set = that sensor answered. These MUST match tb_regs.h's TB_SENSOR_*: the
 * mask is passed through raw, so a private numbering here silently mislabels
 * sensors. They did diverge once -- this list used to read HR/SPO2/RR/ECG/RFID
 * with RFID on 0x10, which is the wire's LoRa bit, so an ECG dropout showed up
 * as an RFID fault. Kept here rather than including tb_regs.h so the desktop
 * sim, which has no link component, exercises the same logic.
 */
#define UI_SENSOR_ECG      0x01U /* AD8232 */
#define UI_SENSOR_MAX30102 0x02U /* HR + SpO2, one sensor */
/*
 * The breathing microphone. A real wire bit, but NOT in UI_SENSOR_ALL yet:
 * the STM32 sets it only under MON_RESP_MIC_FITTED, which is 0 until the mic
 * PCB exists, so a sensor in ALL that nothing can ever report would hold the
 * dot permanently amber for hardware the box does not have. Put it back into
 * ALL the day the board ships.
 */
#define UI_SENSOR_MIC      0x04U /* breathing microphone -> respiratory rate */
#define UI_SENSOR_RFID     0x08U /* PN532 */
/*
 * LoRa is deliberately NOT in UI_SENSOR_ALL even though it shares the byte: it
 * has its own dot, and folding it in here would make a missing radio read as a
 * sensor fault. There is no BP bit either: BP is measured by this board's own
 * BP capture (bp_capture), not by the STM32 this mask reports.
 */
#define UI_SENSOR_LORA     0x10U /* SX1278; reported by the LoRa dot, not Sensor */
#define UI_SENSOR_ALL      (UI_SENSOR_ECG | UI_SENSOR_MAX30102 | UI_SENSOR_RFID)

/* All sensors up = OK, none = ERROR, some = WARN. */
ui_status_state_t ui_status_sensors(uint8_t sensor_ok_mask);

/*
 * "Sistem" dot = is the STM32 link alive? age_ms is how long since the last
 * frame of any kind. never_seen is true before the first frame ever arrives.
 */
ui_status_state_t ui_status_system(uint32_t age_ms, bool never_seen);

/*
 * "LoRa" dot. The radio hangs off the STM32, so the ESP32 only knows what the
 * STM32 tells it: link_ok comes from TB_FRAME_STATUS. Before any STATUS frame
 * arrives we know nothing, which is ERROR, not OK.
 */
ui_status_state_t ui_status_lora(bool link_ok, bool reported);
/* Label text next to each dot: "Sensor OK" / "Sensor !" / "Sensor --". The
 * label mirrors the dot's three states only; it does not count sensors. */
void ui_status_label(char *buf, unsigned buf_sz, const char *prefix,
                     ui_status_state_t state);

/* ---------------------------------------------------- Status bar (all screens) */

/*
 * Battery percent as it should appear in the status bar.
 *
 * UI_BATTERY_UNKNOWN is the wire's "not measured" value, so it renders "--%"
 * rather than a fabricated 0% -- a flat battery and an unread gauge look
 * identical otherwise, and one of them is an emergency.
 */
#define UI_BATTERY_UNKNOWN 0xFFU
void ui_status_battery_text(char *buf, unsigned buf_sz, uint8_t percent);

/*
 * Which of the four authored battery glyphs to show.
 *
 * An index rather than the asset's name string, because the caller has to reach
 * the generated `extern const void *battery_*` globals anyway and a name would
 * mean a lookup table plus a strcmp chain. Keeping this LVGL-free is what lets
 * the host selftest link ui_status.c at all.
 *
 * UI_BATTERY_UNKNOWN maps to EMPTY, not FULL: 0xFF would otherwise sail past the
 * 75% test and paint a full battery for a gauge nobody has read. Erring low is
 * the survivable direction -- it sends someone to find a charger, where erring
 * high strands the box mid-triage. The text beside it reads "--%", so the two
 * together still say "unknown" rather than "nearly flat".
 */
typedef enum {
    UI_BATTERY_ICON_EMPTY,
    UI_BATTERY_ICON_MEDIUM,
    UI_BATTERY_ICON_FULL,
    UI_BATTERY_ICON_CHARGING
} ui_battery_icon_t;

ui_battery_icon_t ui_status_battery_icon(uint8_t percent, bool charging);

/*
 * Status-bar link text, replacing the authored "Connected" literal.
 *
 * Deliberately never says "Connected": the radio hangs off the STM32, so all
 * the ESP32 knows is what TB_REG_SENSOR_OK's LoRa bit claims -- that the SX1278
 * initialised. Nothing here can know a station heard us. Saying "Connected"
 * would be a promise the hardware cannot keep, on a screen someone triages
 * from.
 *
 * reported is false until the first snapshot arrives; that is "no link to the
 * STM32 at all", which is a different fault from "STM32 present, radio down".
 */
/*
 * Status-bar link label, e.g. "-97dBm", "LoRa siap", "LoRa mati", "Link --".
 *
 * Deliberately never says "Connected": the radio hangs off the STM32, so all
 * the ESP32 knows is what TB_REG_SENSOR_OK's LoRa bit claims -- that the SX1278
 * initialised. Nothing here can know a station heard us. Saying "Connected"
 * would be a promise the hardware cannot keep, on a screen someone triages
 * from.
 *
 * reported is false until the first snapshot arrives; that is "no link to the
 * STM32 at all", which is a different fault from "STM32 present, radio down".
 *
 * WHY RSSI GOES IN THIS LABEL rather than a new one: it is the number beside the
 * signal icon, and there is exactly one label there. A second one would need the
 * status_bar.xml component re-exported from the Editor before it existed at all,
 * so the feature would silently do nothing until someone pressed Ctrl+B.
 *
 * Precedence, most actionable first: no STM32 -> radio down -> a measured dBm ->
 * radio up but nothing heard yet. A valid dBm implies a working radio, so the
 * words add nothing once there is a number; and "LoRa mati" outranks a dBm from
 * seconds ago because that is the state worth acting on.
 *
 * No signal-bars mapping anywhere. The number is the point -- this exists so
 * someone can walk the box away from the station and read the actual dBm, and
 * bucketing it into 4 bars throws away exactly the resolution that needs.
 *
 * Needs 12 bytes: the longest output is "-128dBm" plus NUL, with slack.
 */
#define UI_LINK_TEXT_MIN 12U
void ui_status_link_text(char *buf, unsigned buf_sz, bool lora_ok, bool reported,
                         int8_t rssi_dbm, bool rssi_valid);

/*
 * Colour for that label: how much margin is left before the link drops.
 *
 * Thresholds are the receiver's, not a preference. At the shared SF7 / 125 kHz
 * the SX1278 datasheet gives about -123 dBm sensitivity, so:
 *   >= -100  OK     comfortable, tens of dB of margin
 *   >= -115  WARN   under 10 dB left; this is where range testing gets useful
 *   <  -115  ERROR  within a few dB of not decoding at all
 * An unknown RSSI is not an error -- it falls back to the radio's own state,
 * which is what the caller passes in.
 */
#define UI_RSSI_OK_DBM   (-100)
#define UI_RSSI_WARN_DBM (-115)
ui_status_state_t ui_status_rssi_state(int8_t dbm);

/*
 * Clock HH:MM, or "--:--" when the system clock has never been set.
 *
 * The validity test is the clock itself, not a flag someone has to remember to
 * set: an unset ESP32 clock starts at the epoch, so anything before
 * UI_CLOCK_VALID_EPOCH is "no RTC yet". That means fitting the RTC battery and
 * setting the time is all it takes for this to start working -- no code change,
 * which is the point, since the battery is not bought yet.
 *
 * A plain localtime() NULL check does NOT do this: localtime() succeeds on an
 * unset clock and this would confidently display 07:00 on 1 Jan 1970.
 */
#define UI_CLOCK_VALID_EPOCH 1735689600L /* 2025-01-01 UTC; any earlier = unset */
void ui_status_format_clock(char *buf, unsigned buf_sz);

/* Same, with the time passed in, so the "1970 is not a time" rule is testable
 * on a host whose own clock is (correctly) set. */
void ui_status_format_clock_at(char *buf, unsigned buf_sz, time_t now);

#ifdef __cplusplus
}
#endif

#endif
