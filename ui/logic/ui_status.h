#ifndef UI_STATUS_H
#define UI_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *ui_status_battery_icon(uint8_t percent, bool charging);
void ui_status_format_clock(char *buf, unsigned buf_sz);

/* Home screen status dots. */
typedef enum {
    UI_STATUS_OK,
    UI_STATUS_WARN,  /* partially degraded — some sensors down */
    UI_STATUS_ERROR  /* no data at all / link down */
} ui_status_state_t;

/*
 * Sensor bitmask as reported by the STM32 in TB_FRAME_STATUS. Bit set = OK.
 * Kept here rather than in tb_frame.h so the sim can exercise the same logic.
 */
#define UI_SENSOR_HR    0x01U /* MAX30102 */
#define UI_SENSOR_SPO2  0x02U /* MAX30102 */
#define UI_SENSOR_RR    0x04U /* MPX5010DP */
#define UI_SENSOR_ECG   0x08U /* AD8232 */
#define UI_SENSOR_RFID  0x10U /* RC522 */
#define UI_SENSOR_ALL   (UI_SENSOR_HR | UI_SENSOR_SPO2 | UI_SENSOR_RR | \
                         UI_SENSOR_ECG | UI_SENSOR_RFID)

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

/* Label text next to each dot, e.g. "Sensor OK" / "Sensor 3/5" / "Sensor --". */
void ui_status_label(char *buf, unsigned buf_sz, const char *prefix,
                     ui_status_state_t state);

#ifdef __cplusplus
}
#endif

#endif
