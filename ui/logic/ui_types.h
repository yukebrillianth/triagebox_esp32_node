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
 * cable. BP is the exception: it is measured by this board's own BP capture
 * (bp_capture), not reported by the STM32, so its bit is set only for a window
 * the BP task actually published.
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
    /*
     * The rate in `hr` was counted at the FINGER (MAX30102 pulse rate), not at
     * the chest (ECG). The STM32 picks one source per poll and reports which one
     * won; PR <= HR always, because a beat too weak to open the aortic valve is
     * a depolarisation the ECG still sees.
     *
     * Provenance, not freshness -- meaningless unless UI_VITAL_HR is set, and
     * deliberately not a valid_mask bit. tb_ui_source.c is the one consumer: it
     * holds the last ECG rate rather than let a PPG one through while the finger
     * clip on this board revision is unreliable.
     */
    bool hr_from_ppg;
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
 * "The model refused to score this patient", which is carried by the ESI and
 * nothing else.
 *
 * The marker is the ESI, not a fifth ui_priority_t: the enum is the wire order,
 * it indexes designated-initialiser tables, and every switch on it would have to
 * be revisited. tb_classify() answers BLACK with *predicted_esi = 0 when a
 * feature is missing, so on this box BLACK-with-esi-0 is "nobody measured this
 * patient" -- NOT the EXPECTANT of a patient the model looked at and gave up on.
 * Reported as its own neutral state instead, because those two must never look
 * alike on a triage screen: one is a body bag, the other is "measure again".
 */
bool ui_verdict_unscored(int esi);

/*
 * The banner text for a verdict: the colour's label, or the honest neutral one
 * when the model refused.
 *
 * Here rather than in ui_bindings.c so the "a refusal must not read as
 * EXPECTANT" rule is host-testable -- ui_bindings.c needs LVGL and never
 * compiles on the host.
 */
const char *ui_verdict_label(ui_priority_t priority, int esi);

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

/*
 * Respiratory-rate band, entered by the operator counting breaths.
 *
 * A band and not a number because it is a manual count under field conditions:
 * an operator watching a chest for fifteen seconds knows "about twenty", not
 * "seventeen", and four rows are three button presses at worst. ui_rr_band_value()
 * maps each to the breaths/min the model is fed.
 *
 * This exists because nothing on the box measures respiration yet -- the
 * microphone needs a PCB that does not exist -- while tb_classify() refuses to
 * score at all on respiratory_rate <= 0, so every real patient came out BLACK
 * with esi 0. Delete this screen the day the mic ships, not before.
 */
typedef enum {
    UI_RR_BAND_UNDER_12,
    UI_RR_BAND_12_20,
    UI_RR_BAND_21_30,
    UI_RR_BAND_OVER_30
} ui_rr_band_t;

/*
 * Breaths per minute for a band -- the value that reaches
 * TriageInput.respiratory_rate.
 *
 * Each is the clinical midpoint of its band rather than an edge, so a band never
 * scores as the band next door. UI_RR_BAND_12_20 -> 16 sits essentially on the
 * training-set mean (18.14, SD 3.73 -- triage_pipeline.c's own normalisation),
 * which is what makes "normal breathing" the neutral answer it should be.
 */
uint16_t ui_rr_band_value(ui_rr_band_t band);

typedef enum {
    UI_GENDER_M = 'M',
    UI_GENDER_F = 'F',
    UI_GENDER_U = 'U'
} ui_gender_t;

#endif /* UI_LOGIC_UI_TYPES_H */
