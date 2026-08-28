/*
 * Platform-neutral half of the I2C link: decoding a snapshot into vitals_t and
 * turning the button STATE mask into edge events. No ESP-IDF here, so
 * tb_i2c_codec_selftest.c exercises it on the host under ASan/UBSan.
 */
#ifndef TB_I2C_CODEC_H
#define TB_I2C_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tb_regs.h"
#include "ui_types.h"

/*
 * Decode a raw snapshot read. `raw` must be TB_REG_READ_END bytes.
 * Returns false (and leaves *out untouched) if proto_ver does not match --
 * a mismatched tb_regs.h copy between the two repos means every offset below
 * is suspect, so refusing is the only safe answer.
 */
bool tb_i2c_decode_vitals(const uint8_t *raw, vitals_t *out);

/* Sequence counter, for spotting a stalled STM32 superloop (it advances even
 * when no reading changed). */
uint8_t tb_i2c_seq(const uint8_t *raw);

/* Raw button state mask (TB_BTN_*, 1 = pressed). */
uint8_t tb_i2c_buttons(const uint8_t *raw);

/*
 * State-to-edge conversion. The wire carries state because a polled mailbox is
 * idempotent; the UI wants events. Call with the previous mask and the current
 * one; `cb` fires once per changed button.
 *
 * Returns the new mask, to be passed as `prev` next time. On the very first
 * poll pass prev = 0 -- a button already held at boot then reports as a fresh
 * press, which is what the UI should see.
 */
typedef void (*tb_i2c_btn_cb)(uint8_t index, bool pressed, void *ctx);

uint8_t tb_i2c_diff_buttons(uint8_t prev, uint8_t now, tb_i2c_btn_cb cb,
                            void *ctx);

#endif /* TB_I2C_CODEC_H */
