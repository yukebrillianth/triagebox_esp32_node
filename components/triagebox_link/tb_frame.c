#include "tb_frame.h"

#include <string.h>

#include "ui_types.h"

/* CRC-16/CCITT-FALSE. Bitwise: a 512-byte table is not worth it at 115200 baud. */
static uint16_t crc16(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (uint8_t i = 0; i < 8; i++) {
        crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t tb_frame_encode(tb_frame_kind_t kind, const void *payload, uint8_t len,
                       uint8_t *out, size_t out_sz)
{
    uint16_t crc = 0xFFFFU;
    size_t n = 0;

    if (out == NULL || len > TB_FRAME_PAYLOAD_MAX) {
        return 0;
    }
    if (out_sz < (size_t)len + TB_FRAME_OVERHEAD) {
        return 0;
    }
    if (len > 0 && payload == NULL) {
        return 0;
    }

    out[n++] = TB_FRAME_SYNC0;
    out[n++] = TB_FRAME_SYNC1;
    out[n++] = (uint8_t)kind;
    out[n++] = len;
    if (len > 0) {
        memcpy(&out[n], payload, len);
        n += len;
    }

    crc = crc16(crc, (uint8_t)kind);
    crc = crc16(crc, len);
    for (uint8_t i = 0; i < len; i++) {
        crc = crc16(crc, ((const uint8_t *)payload)[i]);
    }
    out[n++] = (uint8_t)(crc & 0xFFU);
    out[n++] = (uint8_t)(crc >> 8);

    return n;
}

enum { S_SYNC0 = 0, S_SYNC1, S_KIND, S_LEN, S_PAYLOAD, S_CRC_LO, S_CRC_HI };

bool tb_frame_feed(tb_frame_parser_t *p, uint8_t byte, tb_frame_t *out)
{
    if (p == NULL) {
        return false;
    }

    switch (p->state) {
    case S_SYNC0:
        if (byte == TB_FRAME_SYNC0) {
            p->state = S_SYNC1;
        }
        break;

    case S_SYNC1:
        /* A repeated 0xA5 is a plausible new frame start, not a desync. */
        if (byte == TB_FRAME_SYNC1) {
            p->state = S_KIND;
        } else if (byte != TB_FRAME_SYNC0) {
            p->state = S_SYNC0;
        }
        break;

    case S_KIND:
        p->kind = byte;
        p->state = S_LEN;
        break;

    case S_LEN:
        if (byte > TB_FRAME_PAYLOAD_MAX) {
            /* Bogus length: treat as noise and hunt for the next sync. */
            p->state = S_SYNC0;
            break;
        }
        p->len = byte;
        p->got = 0;
        p->state = (byte == 0) ? S_CRC_LO : S_PAYLOAD;
        break;

    case S_PAYLOAD:
        p->payload[p->got++] = byte;
        if (p->got == p->len) {
            p->state = S_CRC_LO;
        }
        break;

    case S_CRC_LO:
        p->crc_rx = byte;
        p->state = S_CRC_HI;
        break;

    case S_CRC_HI: {
        uint16_t crc = 0xFFFFU;

        p->crc_rx |= (uint16_t)byte << 8;
        p->state = S_SYNC0;

        crc = crc16(crc, p->kind);
        crc = crc16(crc, p->len);
        for (uint8_t i = 0; i < p->len; i++) {
            crc = crc16(crc, p->payload[i]);
        }
        if (crc != p->crc_rx) {
            p->crc_errors++;
            return false;
        }
        p->frames_ok++;
        if (out != NULL) {
            out->kind = (tb_frame_kind_t)p->kind;
            out->len = p->len;
            memcpy(out->payload, p->payload, p->len);
        }
        return true;
    }

    default:
        p->state = S_SYNC0;
        break;
    }

    return false;
}

/* Index = ui_priority_t (RED,YELLOW,GREEN,BLACK) -> LoRa alias value. */
static const uint8_t k_to_wire[4] = {1U, 2U, 3U, 0U};
/* Index = LoRa alias (BLACK,RED,YELLOW,GREEN) -> ui_priority_t. */
static const uint8_t k_from_wire[4] = {
    (uint8_t)UI_PRIORITY_BLACK,
    (uint8_t)UI_PRIORITY_RED,
    (uint8_t)UI_PRIORITY_YELLOW,
    (uint8_t)UI_PRIORITY_GREEN,
};

uint8_t tb_frame_priority_to_wire(int ui_priority)
{
    if (ui_priority < 0 || ui_priority > (int)UI_PRIORITY_BLACK) {
        return 0U; /* BLACK: safest default if the caller is confused. */
    }
    return k_to_wire[ui_priority];
}

int tb_frame_priority_from_wire(uint8_t wire)
{
    if (wire > 3U) {
        return (int)UI_PRIORITY_BLACK;
    }
    return (int)k_from_wire[wire];
}
