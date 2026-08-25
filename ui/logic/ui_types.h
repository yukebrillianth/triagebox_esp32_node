#ifndef UI_LOGIC_UI_TYPES_H
#define UI_LOGIC_UI_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define RFID_TAG_CAPACITY 32U

/*
 * Which readings in a vitals_t are actually measured right now. One bit per
 * field because the vitals come from different sensors on different cadences:
 * with the ECG unplugged, HR is absent while SpO2 and RR are perfectly good,
 * and blanking all four would hide three working sensors behind one missing
 * cable. BP has no source at all yet, so its bit is always clear.
 */
#define UI_VITAL_HR   0x01U
#define UI_VITAL_SPO2 0x02U
#define UI_VITAL_RR   0x04U
#define UI_VITAL_BP   0x08U

typedef struct {
    uint16_t hr;
    uint16_t spo2;
    uint16_t rr;
    uint16_t bp_sys;
    uint16_t bp_dia;
    uint8_t battery;
    /* Per-field freshness, UI_VITAL_*. Drives what the screens render: a set
     * bit shows the number, a clear one keeps the authored "--". */
    uint8_t valid_mask;
    /*
     * All-or-nothing gate for the SVM, which needs a complete feature vector
     * and must refuse rather than score on absent inputs. NOT the same question
     * as "should this tile show a number" -- use valid_mask for display.
     */
    bool valid;
} vitals_t;

typedef struct {
    uint8_t index; /* 0..3 */
    bool pressed;
    uint32_t timestamp_ms;
} btn_event_t;

typedef struct {
    char tag[RFID_TAG_CAPACITY];
    bool present;
} rfid_t;

/*
 * Health of the STM32 link, for the three Home status dots. The ESP32 cannot
 * see the sensors or the LoRa radio directly — everything here is either
 * reported by the STM32 in a STATUS frame or derived from frame arrival times.
 */
typedef struct {
    uint8_t sensor_mask;    /* UI_SENSOR_* bits from ui_status.h; set = OK */
    bool lora_ok;           /* as reported by the STM32 */
    bool lora_reported;     /* false until the first STATUS frame arrives */
    uint32_t link_age_ms;   /* since the last frame of any kind */
    bool link_never_seen;   /* no frame has ever arrived */
    /*
     * How strongly this node heard the station's last poll, in dBm, and whether
     * that number means anything yet. Downlink direction, measured by the node's
     * own radio -- the only link figure this board can know, and the one the
     * status bar shows while walking the box away to find the range.
     *
     * Kept as a separate flag rather than a magic value because there is no dBm
     * reading that could serve as one: 0 is "no poll heard yet" on a new STM32
     * and -1 is the pad byte an old one returns, and neither is distinguishable
     * from data by looking at it.
     */
    int8_t lora_rssi_dbm;
    bool lora_rssi_valid;
} link_status_t;

typedef enum {
    UI_PRIORITY_RED,
    UI_PRIORITY_YELLOW,
    UI_PRIORITY_GREEN,
    UI_PRIORITY_BLACK
} ui_priority_t;

/* RED: "MERAH - IMMEDIATE"; YELLOW: "KUNING - DELAYED";
 * GREEN: "HIJAU - MINOR"; BLACK: "HITAM - EXPECTANT". */
const char *ui_priority_display_label(ui_priority_t value);

typedef enum {
    UI_AGE_BAND_6_17,
    UI_AGE_BAND_18_45,
    UI_AGE_BAND_46_60,
    UI_AGE_BAND_OVER_60
} ui_age_band_t;

typedef enum {
    UI_GENDER_M = 'M',
    UI_GENDER_F = 'F',
    UI_GENDER_U = 'U'
} ui_gender_t;

#endif /* UI_LOGIC_UI_TYPES_H */
