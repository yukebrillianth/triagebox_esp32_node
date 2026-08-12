/*
 * Host selftest for the RS485 codec. Build+run: tools/run_selftests.sh
 * Not linked into firmware (excluded in CMakeLists.txt).
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tb_frame.h"
#include "ui_types.h"

/* Push every byte of buf; return how many complete frames came out. */
static int feed_all(tb_frame_parser_t *p, const uint8_t *buf, size_t n, tb_frame_t *last)
{
    int frames = 0;
    for (size_t i = 0; i < n; i++) {
        if (tb_frame_feed(p, buf[i], last)) {
            frames++;
        }
    }
    return frames;
}

static void test_roundtrip_vital(void)
{
    /* hr=90 spo2=98 rr=18 bp=120/80 battery=80 valid */
    const uint8_t payload[12] = {
        90, 0, 98, 0, 18, 0, 120, 0, 80, 0, 80, TB_VITAL_FLAG_VALID,
    };
    uint8_t wire[TB_FRAME_MAX];
    tb_frame_parser_t p = {0};
    tb_frame_t got = {0};
    size_t n = tb_frame_encode(TB_FRAME_VITAL, payload, sizeof(payload), wire, sizeof(wire));

    assert(n == sizeof(payload) + TB_FRAME_OVERHEAD);
    assert(feed_all(&p, wire, n, &got) == 1);
    assert(got.kind == TB_FRAME_VITAL);
    assert(got.len == sizeof(payload));
    assert(memcmp(got.payload, payload, sizeof(payload)) == 0);
    assert(p.crc_errors == 0);
}

static void test_zero_length_payload(void)
{
    uint8_t wire[TB_FRAME_MAX];
    tb_frame_parser_t p = {0};
    tb_frame_t got = {0};
    size_t n = tb_frame_encode(TB_FRAME_CMD, NULL, 0, wire, sizeof(wire));

    assert(n == TB_FRAME_OVERHEAD);
    assert(feed_all(&p, wire, n, &got) == 1);
    assert(got.len == 0);
}

static void test_corrupt_crc_rejected(void)
{
    const uint8_t payload[2] = {2, 1};
    uint8_t wire[TB_FRAME_MAX];
    tb_frame_parser_t p = {0};
    size_t n = tb_frame_encode(TB_FRAME_BUTTON, payload, sizeof(payload), wire, sizeof(wire));

    wire[4] ^= 0xFFU; /* flip a payload byte, CRC no longer matches */
    assert(feed_all(&p, wire, n, NULL) == 0);
    assert(p.crc_errors == 1);
    assert(p.frames_ok == 0);
}

static void test_resync_after_garbage(void)
{
    /* Leading noise then a good frame: noise must not prevent the frame. */
    const uint8_t payload[2] = {1, 1};
    uint8_t wire[TB_FRAME_MAX];
    uint8_t stream[TB_FRAME_MAX * 2];
    tb_frame_parser_t p = {0};
    tb_frame_t got = {0};
    size_t n = tb_frame_encode(TB_FRAME_BUTTON, payload, sizeof(payload), wire, sizeof(wire));
    size_t s = 0;

    stream[s++] = 0x00;
    stream[s++] = 0xFF;
    stream[s++] = 0xA5; /* false sync, not followed by 0x5A */
    stream[s++] = 0x13;
    memcpy(&stream[s], wire, n);
    s += n;

    assert(feed_all(&p, stream, s, &got) == 1);
    assert(got.kind == TB_FRAME_BUTTON);
    assert(got.payload[0] == 1);
}

static void test_truncated_frame_eats_the_next_one(void)
{
    /*
     * Documents a real limitation rather than asserting it away: with
     * length-prefixed framing and no byte stuffing, a frame truncated
     * mid-payload makes the parser swallow the following frame's sync bytes as
     * payload, so THAT frame is lost too. The parser still resynchronizes — the
     * frame after it arrives intact — so one dropped frame is the whole cost.
     *
     * Acceptable here: VITAL/STATUS repeat on a cadence, and a lost BUTTON is
     * one missed keypress the operator will repeat. If a frame kind ever
     * becomes must-not-drop, add COBS stuffing so sync can never appear inside
     * a payload.
     */
    const uint8_t payload[2] = {1, 1};
    uint8_t wire[TB_FRAME_MAX];
    uint8_t stream[TB_FRAME_MAX * 4];
    tb_frame_parser_t p = {0};
    tb_frame_t got = {0};
    size_t n = tb_frame_encode(TB_FRAME_BUTTON, payload, sizeof(payload), wire, sizeof(wire));
    size_t s = 0;

    memcpy(&stream[s], wire, n - 3); /* truncated: cut inside the payload */
    s += n - 3;
    memcpy(&stream[s], wire, n);     /* casualty of the truncation above */
    s += n;
    memcpy(&stream[s], wire, n);     /* this one must get through */
    s += n;

    assert(feed_all(&p, stream, s, &got) == 1);
    assert(got.kind == TB_FRAME_BUTTON);
    assert(p.frames_ok == 1);
}

static void test_bogus_length_does_not_overflow(void)
{
    /* A corrupted len byte must not make the parser write past payload[]. */
    uint8_t stream[6] = {TB_FRAME_SYNC0, TB_FRAME_SYNC1, TB_FRAME_VITAL, 0xFFU, 0x00, 0x00};
    tb_frame_parser_t p = {0};

    assert(feed_all(&p, stream, sizeof(stream), NULL) == 0);
    assert(p.crc_errors == 0); /* rejected at length, never reached CRC */
}

static void test_encode_rejects_bad_args(void)
{
    uint8_t small[4];
    uint8_t big[TB_FRAME_MAX];
    const uint8_t payload[2] = {0, 0};

    assert(tb_frame_encode(TB_FRAME_BUTTON, payload, sizeof(payload), small, sizeof(small)) == 0);
    assert(tb_frame_encode(TB_FRAME_RFID, payload, TB_FRAME_PAYLOAD_MAX + 1, big, sizeof(big)) == 0);
    assert(tb_frame_encode(TB_FRAME_VITAL, NULL, 4, big, sizeof(big)) == 0);
}

static void test_priority_wire_mapping(void)
{
    /* The one thing most likely to be wired backwards: ui_priority_t order is
     * RED,YELLOW,GREEN,BLACK but the wire is 0=BLACK,1=RED,2=YELLOW,3=GREEN. */
    assert(tb_frame_priority_to_wire(UI_PRIORITY_BLACK) == 0);
    assert(tb_frame_priority_to_wire(UI_PRIORITY_RED) == 1);
    assert(tb_frame_priority_to_wire(UI_PRIORITY_YELLOW) == 2);
    assert(tb_frame_priority_to_wire(UI_PRIORITY_GREEN) == 3);

    for (int prio = UI_PRIORITY_RED; prio <= UI_PRIORITY_BLACK; prio++) {
        assert(tb_frame_priority_from_wire(tb_frame_priority_to_wire(prio)) == prio);
    }
    /* Out-of-range in either direction degrades to BLACK, never to RED. */
    assert(tb_frame_priority_to_wire(99) == 0);
    assert(tb_frame_priority_from_wire(200) == UI_PRIORITY_BLACK);
}

int main(void)
{
    test_roundtrip_vital();
    test_zero_length_payload();
    test_corrupt_crc_rejected();
    test_resync_after_garbage();
    test_truncated_frame_eats_the_next_one();
    test_bogus_length_does_not_overflow();
    test_encode_rejects_bad_args();
    test_priority_wire_mapping();
    printf("tb_frame_selftest: OK\n");
    return 0;
}
