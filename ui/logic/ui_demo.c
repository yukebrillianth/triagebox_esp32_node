#include "ui_demo.h"

#include <stddef.h>

#include "ui_status.h" /* UI_SENSOR_ALL -- the same mask the STM32 publishes */

static bool s_enabled;

bool ui_demo_enabled(void)
{
    return s_enabled;
}

void ui_demo_set(bool on)
{
    s_enabled = on;
}

void ui_demo_toggle(void)
{
    s_enabled = !s_enabled;
}

/*
 * One wobble shape, sampled at different phases per field so the four numbers do
 * not step together -- readings that move in lockstep look like one counter
 * driving all of them. Integer table rather than a sine: no float, no libm, and
 * the same value for the same now_ms, which is what makes the selftest possible.
 */
static const int8_t k_wobble[8] = {0, 2, 3, 1, 0, -2, -3, -1};

#define DEMO_STEP_MS 800U /* one step ~ a breath; slow enough to read on video */

static uint16_t drift(uint32_t now_ms, uint8_t phase, uint16_t base, uint8_t gain)
{
    uint32_t step = (now_ms / DEMO_STEP_MS) + phase;

    return (uint16_t)((int)base + k_wobble[step & 7U] * (int)gain);
}

void ui_demo_vitals(uint32_t now_ms, vitals_t *out)
{
    if (out == NULL) {
        return;
    }
    /* Bands, at the widest wobble: HR 122-134, SpO2 85-91, RR 29-35,
     * BP 80-92 / 51-57. Every one stays well clear of 0, so no clamp is needed
     * and no field can accidentally land on the "sensor missing" sentinel. */
    out->hr     = drift(now_ms, 0U, 128U, 2U);
    out->spo2   = drift(now_ms, 3U,  88U, 1U);
    out->rr     = drift(now_ms, 5U,  32U, 1U);
    out->bp_sys = drift(now_ms, 2U,  86U, 2U);
    out->bp_dia = drift(now_ms, 6U,  54U, 1U);

    /* All four tiles show numbers, and the all-or-nothing gate is open -- but
     * nothing downstream actually scores these: the caller substitutes
     * UI_DEMO_PRIORITY instead of running the model. Set anyway so the screens
     * behave exactly as they do with a complete real snapshot. */
    out->valid_mask = UI_VITAL_HR | UI_VITAL_SPO2 | UI_VITAL_RR | UI_VITAL_BP;
    out->valid = true;
    /* The demo patient is fully instrumented, so its HR is the one the chest
     * leads would give -- the tile prints "EKG" rather than "jari", which is
     * also what a filmed take should show. */
    out->hr_from_ppg = false;
}

uint8_t ui_demo_sensor_mask(void)
{
    return UI_SENSOR_ALL;
}
