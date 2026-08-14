#ifndef TB_FRAME_H
#define TB_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * RS485 wire format, STM32 <-> ESP32. Little-endian.
 *
 *   0xA5 0x5A | kind:u8 | len:u8 | payload[len] | crc16:u16
 *
 * CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over kind+len+payload.
 * Explicit framing because RS485 drops bytes on a noisy line: a receiver that
 * loses sync must be able to find the next frame without a reset.
 *
 * No malloc, no ESP-IDF dependency — the STM32 side can compile this file
 * verbatim, and tb_frame_selftest.c runs it on the host.
 */

#define TB_FRAME_SYNC0 0xA5U
#define TB_FRAME_SYNC1 0x5AU

/* Longest payload is RFID (31 ASCII bytes). */
#define TB_FRAME_PAYLOAD_MAX 32U
#define TB_FRAME_OVERHEAD    6U /* sync0+sync1+kind+len+crc_lo+crc_hi */
#define TB_FRAME_MAX         (TB_FRAME_PAYLOAD_MAX + TB_FRAME_OVERHEAD)

typedef enum {
    TB_FRAME_VITAL  = 0x01, /* hr,spo2,rr,bp_sys,bp_dia:u16 + battery:u8 + flags:u8 */
    TB_FRAME_BUTTON = 0x02, /* index:u8 (0..3) + pressed:u8 */
    TB_FRAME_RFID   = 0x03, /* tag[len] ASCII, len <= 31, not NUL-terminated */
    TB_FRAME_STATUS = 0x04, /* sensor_ok:u8 bitmask + battery:u8 [+ lora_ok:u8] */
    TB_FRAME_CMD    = 0x10, /* cmd:u8, see tb_cmd_t */
    TB_FRAME_RESULT = 0x11  /* priority:u8 (LoRa order!) + confidence:u8 (0..100) + tag[] */
} tb_frame_kind_t;

typedef enum {
    TB_CMD_START_SCAN    = 0x01,
    TB_CMD_START_MEASURE = 0x02,
    TB_CMD_ABORT         = 0x03,
    TB_CMD_POWER_OFF     = 0x04
} tb_cmd_t;

/* TB_FRAME_VITAL flags bit 0: readings are fresh and trustworthy. */
#define TB_VITAL_FLAG_VALID 0x01U

typedef struct {
    tb_frame_kind_t kind;
    uint8_t len;
    uint8_t payload[TB_FRAME_PAYLOAD_MAX];
} tb_frame_t;

/*
 * Serialize into out (needs len + TB_FRAME_OVERHEAD bytes).
 * Returns bytes written, or 0 if len > TB_FRAME_PAYLOAD_MAX or out is too small.
 */
size_t tb_frame_encode(tb_frame_kind_t kind, const void *payload, uint8_t len,
                       uint8_t *out, size_t out_sz);

/* Byte-at-a-time parser state. Zero-initialize before first use. */
typedef struct {
    uint8_t state;
    uint8_t kind;
    uint8_t len;
    uint8_t got;
    uint8_t payload[TB_FRAME_PAYLOAD_MAX];
    uint16_t crc_rx;
    /* Diagnostics: a rising crc_errors is the first sign of a bad line. */
    uint32_t crc_errors;
    uint32_t frames_ok;
} tb_frame_parser_t;

/*
 * Feed one received byte. Returns true and fills *out when a frame with a good
 * CRC completes; returns false otherwise (mid-frame, resync, or CRC failure).
 */
bool tb_frame_feed(tb_frame_parser_t *p, uint8_t byte, tb_frame_t *out);

/*
 * Priority on the wire uses the LoRa numeric alias from AGENTS.md
 * (0=BLACK, 1=RED, 2=YELLOW, 3=GREEN), which is NOT ui_priority_t's
 * declaration order (RED, YELLOW, GREEN, BLACK). Always convert here.
 */
uint8_t tb_frame_priority_to_wire(int ui_priority);
int tb_frame_priority_from_wire(uint8_t wire);

#endif /* TB_FRAME_H */
