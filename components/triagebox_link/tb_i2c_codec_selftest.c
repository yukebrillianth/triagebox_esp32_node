/*
 * Host selftest for the platform-neutral half of the I2C link: snapshot decode
 * and the button state-to-edge diff. Compiles the real tb_i2c_codec.c -- no
 * fakes needed, because that file deliberately has no ESP-IDF in it.
 *
 * The offsets themselves are pinned on the STM32 side too
 * (tools/tb_link_selftest.c there, over the same tb_regs.h). Both repos check
 * the same contract from their own end, which is the point: if someone edits
 * one copy of tb_regs.h, the other repo's test fails.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "tb_i2c_codec.h"
#include "tb_regs.h"
#include "ui_types.h"

/* Build a snapshot the way the STM32 would, so the test exercises real offsets
 * rather than trusting the struct. */
static void put16(uint8_t *raw, uint8_t reg, uint16_t val)
{
    raw[reg] = (uint8_t)(val & 0xFFU);
    raw[reg + 1U] = (uint8_t)(val >> 8);
}

static void make_snapshot(uint8_t *raw)
{
    memset(raw, 0, TB_REG_READ_END);
    raw[TB_REG_PROTO_VER] = TB_PROTO_VER;
    raw[TB_REG_SEQ] = 7;
    raw[TB_REG_FLAGS] = TB_FLAG_HR_VALID | TB_FLAG_SPO2_VALID | TB_FLAG_RR_VALID;
    put16(raw, TB_REG_HR, 78);
    put16(raw, TB_REG_SPO2, 97);
    put16(raw, TB_REG_RR_X10, 165); /* 16.5 breaths/min */
    raw[TB_REG_BATTERY] = 0xFFU;
    raw[TB_REG_SENSOR_OK] = TB_SENSOR_ECG | TB_SENSOR_MAX30102 | TB_SENSOR_MIC;
}

static void test_decode_basic(void)
{
    uint8_t raw[TB_REG_READ_END];
    vitals_t v;

    make_snapshot(raw);
    memset(&v, 0xAA, sizeof(v));

    assert(tb_i2c_decode_vitals(raw, &v));
    assert(v.hr == 78);
    assert(v.spo2 == 97);
    /* 16.5 rounds to 17, not truncates to 16. */
    assert(v.rr == 17);
    assert(v.battery == 0xFFU);
    assert(v.valid);

    /* BP absent and honest: zeros, and validity does not depend on it. */
    assert(v.bp_sys == 0);
    assert(v.bp_dia == 0);
}

static void test_decode_endianness(void)
{
    uint8_t raw[TB_REG_READ_END];
    vitals_t v;

    make_snapshot(raw);
    /* 0x0102 = 258: catches a byte-swapped read, which a small value would not
     * (78 and 0x4E00 both "look plausible" only if you check the number). */
    put16(raw, TB_REG_HR, 0x0102U);
    assert(raw[TB_REG_HR] == 0x02U);      /* low byte first on the wire */
    assert(raw[TB_REG_HR + 1U] == 0x01U);
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(v.hr == 0x0102U);
}

static void test_decode_rejects_wrong_version(void)
{
    uint8_t raw[TB_REG_READ_END];
    vitals_t v;
    vitals_t before;

    make_snapshot(raw);
    raw[TB_REG_PROTO_VER] = (uint8_t)(TB_PROTO_VER + 1U);

    memset(&v, 0x5A, sizeof(v));
    before = v;
    assert(!tb_i2c_decode_vitals(raw, &v));
    /* Must leave the output alone, not half-fill it: a caller that ignores the
     * return value should keep its previous good reading, not get garbage. */
    assert(memcmp(&v, &before, sizeof(v)) == 0);
}

static void test_decode_validity_rules(void)
{
    uint8_t raw[TB_REG_READ_END];
    vitals_t v;

    /* HR alone is not enough. */
    make_snapshot(raw);
    raw[TB_REG_FLAGS] = TB_FLAG_HR_VALID;
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(!v.valid);

    /* SpO2 alone is not enough. */
    raw[TB_REG_FLAGS] = TB_FLAG_SPO2_VALID;
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(!v.valid);

    /* HR + SpO2 is, even with RR missing. */
    raw[TB_REG_FLAGS] = TB_FLAG_HR_VALID | TB_FLAG_SPO2_VALID;
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(v.valid);

    /*
     * And BP being absent must NOT invalidate. Nothing measures pressure yet;
     * if validity required TB_FLAG_BP_VALID every measurement would read
     * invalid forever and the Monitor screen would never show a number.
     */
    assert((raw[TB_REG_FLAGS] & TB_FLAG_BP_VALID) == 0U);
    assert(v.valid);
}

/*
 * The display mask is per-field and must NOT collapse into `valid`. This is the
 * regression: with the ECG unplugged the STM32 reports SpO2 and RR but no HR,
 * `valid` goes false, and every tile on the Monitor screen went blank -- three
 * working sensors hidden by one missing cable.
 */
static void test_decode_valid_mask_is_per_field(void)
{
    uint8_t raw[TB_REG_READ_END];
    vitals_t v;

    make_snapshot(raw);
    raw[TB_REG_FLAGS] = TB_FLAG_SPO2_VALID | TB_FLAG_RR_VALID; /* no ECG */
    assert(tb_i2c_decode_vitals(raw, &v));

    assert(!v.valid); /* SVM still refuses: HR is a feature it needs */
    assert((v.valid_mask & UI_VITAL_SPO2) != 0U);
    assert((v.valid_mask & UI_VITAL_RR) != 0U);
    assert((v.valid_mask & UI_VITAL_HR) == 0U);
    assert((v.valid_mask & UI_VITAL_BP) == 0U);

    /* Every flag set maps to every bit set, and BP tracks its own wire bit. */
    raw[TB_REG_FLAGS] = TB_FLAG_HR_VALID | TB_FLAG_SPO2_VALID |
                        TB_FLAG_RR_VALID | TB_FLAG_BP_VALID;
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(v.valid_mask == (UI_VITAL_HR | UI_VITAL_SPO2 | UI_VITAL_RR |
                            UI_VITAL_BP));

    /* Nothing measured: no tile shows a number. */
    raw[TB_REG_FLAGS] = 0U;
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(v.valid_mask == 0U);
    assert(!v.valid);

    /*
     * TB_FLAG_PPG_CONTACT must NOT reach valid_mask. It says a finger is on the
     * MAX30102, which is a statement about the waveform ring, not about any
     * displayed number -- a finger touching the sensor does not mean an SpO2
     * block has been computed yet. Folding it in would put a stale or unset
     * percentage on the screen the moment someone touched the sensor.
     */
    raw[TB_REG_FLAGS] = TB_FLAG_PPG_CONTACT;
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(v.valid_mask == 0U);
    assert(!v.valid);

    /* And it does not disturb the bits that are set alongside it. */
    raw[TB_REG_FLAGS] = TB_FLAG_PPG_CONTACT | TB_FLAG_SPO2_VALID;
    assert(tb_i2c_decode_vitals(raw, &v));
    assert(v.valid_mask == UI_VITAL_SPO2);
}

static void test_decode_null_safe(void)
{
    uint8_t raw[TB_REG_READ_END];
    vitals_t v;

    make_snapshot(raw);
    assert(!tb_i2c_decode_vitals(NULL, &v));
    assert(!tb_i2c_decode_vitals(raw, NULL));
    assert(tb_i2c_seq(NULL) == 0U);
    assert(tb_i2c_buttons(NULL) == 0U);
}

static void test_buttons_masked(void)
{
    uint8_t raw[TB_REG_READ_END];

    make_snapshot(raw);
    /* A garbled read must not invent buttons 4..7. */
    raw[TB_REG_BUTTONS] = 0xFFU;
    assert(tb_i2c_buttons(raw) == (TB_BTN_1 | TB_BTN_2 | TB_BTN_3 | TB_BTN_4));
}

/* ---- state -> edge ------------------------------------------------------- */

#define MAX_EV 8
static struct {
    uint8_t index;
    bool pressed;
} s_ev[MAX_EV];
static int s_n;

static void rec(uint8_t index, bool pressed, void *ctx)
{
    assert(ctx == NULL);
    assert(s_n < MAX_EV);
    s_ev[s_n].index = index;
    s_ev[s_n].pressed = pressed;
    ++s_n;
}

static void test_diff_single(void)
{
    uint8_t prev = 0;

    s_n = 0;
    prev = tb_i2c_diff_buttons(prev, TB_BTN_1, rec, NULL);
    assert(s_n == 1);
    assert(s_ev[0].index == 0 && s_ev[0].pressed);
    assert(prev == TB_BTN_1);

    /* Unchanged state must emit nothing -- the whole point of state on the wire
     * is that repeated identical polls are free. */
    s_n = 0;
    prev = tb_i2c_diff_buttons(prev, TB_BTN_1, rec, NULL);
    assert(s_n == 0);

    /* Release. */
    s_n = 0;
    prev = tb_i2c_diff_buttons(prev, 0, rec, NULL);
    assert(s_n == 1);
    assert(s_ev[0].index == 0 && !s_ev[0].pressed);
    assert(prev == 0);
}

static void test_diff_simultaneous(void)
{
    uint8_t prev = 0;

    /*
     * Two buttons changing in one poll. This is the case the old single-slot
     * buffer dropped, and it is reachable in practice: press two keys within
     * one 50 ms poll, or have the UI miss a poll while redrawing.
     */
    s_n = 0;
    prev = tb_i2c_diff_buttons(prev, TB_BTN_1 | TB_BTN_3, rec, NULL);
    assert(s_n == 2);
    /* Ascending index order, deterministic. */
    assert(s_ev[0].index == 0 && s_ev[0].pressed);
    assert(s_ev[1].index == 2 && s_ev[1].pressed);
    assert(prev == (TB_BTN_1 | TB_BTN_3));

    /* One releases while the other stays down: exactly one event. */
    s_n = 0;
    prev = tb_i2c_diff_buttons(prev, TB_BTN_3, rec, NULL);
    assert(s_n == 1);
    assert(s_ev[0].index == 0 && !s_ev[0].pressed);
    assert(prev == TB_BTN_3);
}

static void test_diff_all_four(void)
{
    uint8_t prev = 0;
    const uint8_t all = TB_BTN_1 | TB_BTN_2 | TB_BTN_3 | TB_BTN_4;
    int i;

    s_n = 0;
    prev = tb_i2c_diff_buttons(prev, all, rec, NULL);
    assert(s_n == 4);
    for (i = 0; i < 4; i++) {
        assert(s_ev[i].index == (uint8_t)i);
        assert(s_ev[i].pressed);
    }

    s_n = 0;
    prev = tb_i2c_diff_buttons(prev, 0, rec, NULL);
    assert(s_n == 4);
    for (i = 0; i < 4; i++) {
        assert(!s_ev[i].pressed);
    }
    assert(prev == 0);
}

static void test_diff_ignores_high_bits(void)
{
    uint8_t prev;

    /* High bits in either argument must produce no events and must not appear
     * in the returned state. */
    s_n = 0;
    prev = tb_i2c_diff_buttons(0xF0U, 0xF0U, rec, NULL);
    assert(s_n == 0);
    assert(prev == 0);

    s_n = 0;
    prev = tb_i2c_diff_buttons(0x00U, 0xF0U, rec, NULL);
    assert(s_n == 0);
    assert(prev == 0);
}

static void test_diff_null_cb(void)
{
    /* Used to reseed the baseline without emitting events. */
    assert(tb_i2c_diff_buttons(0, TB_BTN_2, NULL, NULL) == TB_BTN_2);
}

/*
 * RSSI lives OUTSIDE the vitals block, and that separation is the whole reason
 * an old STM32 keeps working. Pinned here because the failure mode is silent in
 * both directions: fold it inside and every vitals poll asks an old slave for
 * one byte more than it has (a link that looks dead); get the validity window
 * wrong and the pad byte an old slave returns renders as a signal strength.
 */
static void test_rssi_is_outside_the_vitals_block(void)
{
    /* The vitals poll length must not have moved -- an STM32 built before this
     * register existed answers exactly TB_REG_READ_END bytes. */
    assert(TB_REG_READ_END == 0x30U);
    assert(TB_REG_LORA_RSSI == TB_REG_READ_END);
    assert(TB_REG_SNAPSHOT_END == TB_REG_READ_END + 1U);
    /* The struct is what sizes the slave's staging buffer, so 0x30 is only
     * readable if the member is in it. */
    assert(sizeof(tb_snapshot_t) == TB_REG_SNAPSHOT_END);
    assert(offsetof(tb_snapshot_t, lora_rssi) == TB_REG_LORA_RSSI);
    /* Clear of the PPG block and the write block, both of which the slave
     * decodes by address range. */
    assert(TB_REG_SNAPSHOT_END <= TB_REG_CMD);
    assert(TB_REG_SNAPSHOT_END <= TB_REG_PPG_BASE);
}

static void test_rssi_validity_window(void)
{
    /* Both "no reading" bytes must be rejected, and they are different values
     * from different places: 0 is a fresh STM32 that has heard no poll yet,
     * 0xFF (-1) is the pad an old slave feeds for an address it cannot decode.
     * A single-sentinel test would let one of them through as a real level. */
    assert(!tb_rssi_valid(0));
    assert(!tb_rssi_valid((int8_t)0xFFU));

    /* Real receiver range: SF7/125k sensitivity is about -123 dBm, and closer
     * than a metre saturates near -20. */
    assert(tb_rssi_valid(-123));
    assert(tb_rssi_valid(TB_RSSI_MIN_DBM));
    assert(tb_rssi_valid(TB_RSSI_MAX_DBM));
    assert(!tb_rssi_valid(TB_RSSI_MAX_DBM + 1));

    /* The lower bound is the TYPE, not a comparison: int8_t cannot represent
     * anything below -128, which is why tb_rssi_valid() only tests the top end.
     * Pinned so widening the field notices that the check has to grow back. */
    assert(TB_RSSI_MIN_DBM == -128);
    assert(sizeof(((tb_snapshot_t *)0)->lora_rssi) == 1U);
}

int main(void)
{
    test_decode_basic();
    test_decode_endianness();
    test_decode_rejects_wrong_version();
    test_decode_validity_rules();
    test_decode_valid_mask_is_per_field();
    test_decode_null_safe();
    test_rssi_is_outside_the_vitals_block();
    test_rssi_validity_window();
    test_buttons_masked();
    test_diff_single();
    test_diff_simultaneous();
    test_diff_all_four();
    test_diff_ignores_high_bits();
    test_diff_null_cb();

    printf("tb_i2c_codec_selftest: all checks passed\n");
    return 0;
}
