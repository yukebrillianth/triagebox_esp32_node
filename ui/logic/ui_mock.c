#include "ui_mock.h"

#include <string.h>

#include "ui_status.h" /* UI_SENSOR_* bits */

/* Fixed mock RFID for all QA (matches backend demo seed). */
static const char k_mock_rfid[] = "3021";

/* Base vitals (canonical backend keys). Jitter applied on get. */
static const vitals_t k_base_vitals = {
    .hr = 90,
    .spo2 = 98,
    .rr = 18,
    .bp_sys = 120,
    .bp_dia = 80,
    .battery = 80,
    .valid = true,
};

/* Hardcoded confidence/reasons per priority — no triage tree. */
static const float k_confidence[] = {
    [UI_PRIORITY_RED] = 0.95f,
    [UI_PRIORITY_YELLOW] = 0.85f,
    [UI_PRIORITY_GREEN] = 0.90f,
    [UI_PRIORITY_BLACK] = 0.80f,
};

static const char *const k_reasons[] = {
    [UI_PRIORITY_RED] = "HR>130",
    [UI_PRIORITY_YELLOW] = "SpO2<94",
    [UI_PRIORITY_GREEN] = "",
    [UI_PRIORITY_BLACK] = "unresponsive",
};

/* Cycle order for QA Result variants (not enum declaration order). */
static const ui_priority_t k_cycle[] = {
    UI_PRIORITY_GREEN,
    UI_PRIORITY_YELLOW,
    UI_PRIORITY_RED,
    UI_PRIORITY_BLACK,
};

static uint32_t s_now_ms;
static uint8_t s_cycle_idx; /* index into k_cycle */

/* Scan state */
static bool s_scan_active;
static uint32_t s_scan_start_ms;
static bool s_rfid_ready;
static rfid_t s_rfid;

/* Measure state */
static bool s_measure_active;
static bool s_measure_done;
static uint32_t s_measure_start_ms;
static uint8_t s_measure_progress;

/* Vitals jitter PRNG (xorshift32) — deterministic, no libc rand. */
static uint32_t s_rng = 0xA5A5u;

/* Button single-slot buffer */
static bool s_btn_pending;
static btn_event_t s_btn;

static uint32_t mock_rand(void)
{
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x ? x : 1u;
    return s_rng;
}

static int16_t mock_jitter(int16_t span)
{
    /* Uniform in [-span, +span]. */
    uint32_t r = mock_rand();
    return (int16_t)((int32_t)(r % (uint32_t)(2 * span + 1)) - span);
}

void ui_mock_init(void)
{
    s_now_ms = 0;
    s_cycle_idx = 0;
    s_scan_active = false;
    s_scan_start_ms = 0;
    s_rfid_ready = false;
    memset(&s_rfid, 0, sizeof(s_rfid));
    s_measure_active = false;
    s_measure_done = false;
    s_measure_start_ms = 0;
    s_measure_progress = 0;
    s_rng = 0xA5A5u;
    s_btn_pending = false;
    memset(&s_btn, 0, sizeof(s_btn));
}

void ui_mock_tick(uint32_t now_ms)
{
    s_now_ms = now_ms;

    if (s_scan_active && !s_rfid_ready) {
        if ((now_ms - s_scan_start_ms) >= UI_MOCK_SCAN_MS) {
            memset(&s_rfid, 0, sizeof(s_rfid));
            /* k_mock_rfid fits RFID_TAG_CAPACITY with room. */
            memcpy(s_rfid.tag, k_mock_rfid, sizeof(k_mock_rfid));
            s_rfid.present = true;
            s_rfid_ready = true;
            s_scan_active = false;
        }
    }

    if (s_measure_active && !s_measure_done) {
        uint32_t elapsed = now_ms - s_measure_start_ms;
        if (elapsed >= UI_MEASURE_MS) {
            s_measure_progress = 100;
            s_measure_done = true;
            s_measure_active = false;
        } else {
            s_measure_progress = (uint8_t)((elapsed * 100U) / UI_MEASURE_MS);
        }
    }
}

void ui_mock_start_scan(void)
{
    s_scan_active = true;
    s_scan_start_ms = s_now_ms;
    s_rfid_ready = false;
    memset(&s_rfid, 0, sizeof(s_rfid));
}

bool ui_mock_rfid_ready(rfid_t *out)
{
    if (!s_rfid_ready) {
        return false;
    }
    if (out != NULL) {
        *out = s_rfid;
    }
    /* One-shot: consume ready flag so caller sees it once. */
    s_rfid_ready = false;
    return true;
}

void ui_mock_start_measure(void)
{
    s_measure_active = true;
    s_measure_done = false;
    s_measure_start_ms = s_now_ms;
    s_measure_progress = 0;
}

uint8_t ui_mock_measure_progress(void)
{
    return s_measure_progress;
}

bool ui_mock_measure_done(void)
{
    return s_measure_done;
}

void ui_mock_get_vitals(vitals_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = k_base_vitals;
    /* Small jitter for Monitor "live" feel; keep values in plausible range. */
    out->hr = (uint16_t)((int16_t)k_base_vitals.hr + mock_jitter(3));
    out->spo2 = (uint16_t)((int16_t)k_base_vitals.spo2 + mock_jitter(1));
    out->rr = (uint16_t)((int16_t)k_base_vitals.rr + mock_jitter(2));
    out->bp_sys = (uint16_t)((int16_t)k_base_vitals.bp_sys + mock_jitter(4));
    out->bp_dia = (uint16_t)((int16_t)k_base_vitals.bp_dia + mock_jitter(3));
    /* battery stays flat for QA stability */
    out->valid = true;
}

ui_priority_t ui_mock_get_priority(void)
{
    return k_cycle[s_cycle_idx % 4u];
}

float ui_mock_get_confidence(void)
{
    return k_confidence[ui_mock_get_priority()];
}

const char *ui_mock_get_reasons(void)
{
    return k_reasons[ui_mock_get_priority()];
}

void ui_mock_cycle_priority(void)
{
    s_cycle_idx = (uint8_t)((s_cycle_idx + 1u) % 4u);
}

void ui_mock_push_button(uint8_t index, bool pressed)
{
    s_btn.index = (uint8_t)(index & 0x03u);
    s_btn.pressed = pressed;
    s_btn.timestamp_ms = s_now_ms;
    s_btn_pending = true;
}

bool ui_mock_pop_button(btn_event_t *out)
{
    if (!s_btn_pending) {
        return false;
    }
    if (out != NULL) {
        *out = s_btn;
    }
    s_btn_pending = false;
    return true;
}

void ui_mock_get_link_status(link_status_t *out)
{
    if (out == NULL) {
        return;
    }
    /* Desktop QA has no STM32: report a fully healthy link so the Home dots
     * are exercised in their green state. The device implementation in
     * tb_ui_source.c reports what actually arrived. */
    out->sensor_mask = UI_SENSOR_ALL;
    out->lora_ok = true;
    out->lora_reported = true;
    out->link_age_ms = 0;
    out->link_never_seen = false;
}

void ui_mock_power_off(void)
{
    /* Nothing to power down in the sim; the device build sends POWER_OFF to the
     * STM32, which holds the rail. */
}
