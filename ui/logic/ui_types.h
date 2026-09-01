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
    /*
     * Poll counters, for the range-test screen. These are the ESP32's own view of
     * the I2C link -- successful snapshot polls and failed ones -- not the STM32's
     * LoRa packet counters, which no register exposes. They ride in this struct
     * because it is already the "what does the ESP32 know about the link" carrier
     * and both ui_mock.h implementations fill it; a separate accessor would have
     * to be a tb_link_* call, which the sim cannot link.
     */
    uint32_t polls_ok;
    uint32_t polls_failed;
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

/*
 * Did the patient get worse? True when `to` is more severe than `from`.
 *
 * A separate function and not `to < from`, because ui_priority_t's numbering is
 * NOT severity order -- RED is 0 and BLACK is 3 (tb_triage.h says the same about
 * casting ESI). The rank used here is GREEN < YELLOW < RED < BLACK, so every move
 * toward BLACK counts as a degradation: HITAM is where a patient who stopped
 * breathing lands, which is the one transition an alarm must never miss.
 *
 * Equal is not degraded, so a re-triage that agrees with the last one is silent.
 */
bool ui_priority_degraded(ui_priority_t from, ui_priority_t to);

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
