#ifndef UI_LOGIC_UI_TYPES_H
#define UI_LOGIC_UI_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define RFID_TAG_CAPACITY 32U

typedef struct {
    uint16_t hr;
    uint16_t spo2;
    uint16_t rr;
    uint16_t bp_sys;
    uint16_t bp_dia;
    uint8_t battery;
    /* True only while these readings are valid and not stale. */
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
