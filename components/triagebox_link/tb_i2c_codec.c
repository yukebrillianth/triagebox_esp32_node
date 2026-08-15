#include "tb_i2c_codec.h"

/* Little-endian u16 at a register offset. Shifts rather than memcpy so this is
 * correct on a big-endian host too -- the selftests run on the build machine,
 * not on the target. */
static uint16_t rd16(const uint8_t *raw, uint8_t reg)
{
    return (uint16_t)((uint16_t)raw[reg] | ((uint16_t)raw[reg + 1U] << 8));
}

bool tb_i2c_decode_vitals(const uint8_t *raw, vitals_t *out)
{
    uint8_t flags;

    if ((raw == NULL) || (out == NULL)) {
        return false;
    }
    if (raw[TB_REG_PROTO_VER] != TB_PROTO_VER) {
        return false;
    }

    flags = raw[TB_REG_FLAGS];

    out->hr = rd16(raw, TB_REG_HR);
    out->spo2 = rd16(raw, TB_REG_SPO2);
    /*
     * rr_x10 is tenths on the wire (the STM32's DSP resolves fractional breaths
     * per minute); vitals_t.rr is whole breaths/min. Round rather than truncate
     * so 17.6 does not display as 17.
     */
    out->rr = (uint16_t)((rd16(raw, TB_REG_RR_X10) + 5U) / 10U);
    out->bp_sys = rd16(raw, TB_REG_BP_SYS);
    out->bp_dia = rd16(raw, TB_REG_BP_DIA);
    out->battery = raw[TB_REG_BATTERY];

    /*
     * vitals_t has one `valid` flag but the wire has one bit per vital, because
     * the readings come from different sensors on different cadences. Collapse
     * conservatively: valid only when heart rate AND SpO2 are both good, since
     * those are the two the SVM weighs most and the two the operator reads off
     * the Monitor screen. RR alone is not enough to call a measurement good.
     *
     * BP is deliberately NOT required: nothing measures pressure yet, so
     * demanding TB_FLAG_BP_VALID would make every measurement invalid forever.
     * The zeros in bp_sys/bp_dia are honest -- TB_FLAG_BP_VALID stays clear --
     * and tb_svm_classify() is what has to decide whether to score on 3 of 5
     * features. That is a model question, flagged in the docs, not something to
     * paper over here.
     */
    out->valid = ((flags & TB_FLAG_HR_VALID) != 0U) &&
                 ((flags & TB_FLAG_SPO2_VALID) != 0U);

    return true;
}

uint8_t tb_i2c_seq(const uint8_t *raw)
{
    return (raw != NULL) ? raw[TB_REG_SEQ] : 0U;
}

uint8_t tb_i2c_buttons(const uint8_t *raw)
{
    /* Mask to the four real buttons: a garbled read must not invent keys. */
    return (raw != NULL)
               ? (uint8_t)(raw[TB_REG_BUTTONS] &
                           (TB_BTN_1 | TB_BTN_2 | TB_BTN_3 | TB_BTN_4))
               : 0U;
}

uint8_t tb_i2c_diff_buttons(uint8_t prev, uint8_t now, tb_i2c_btn_cb cb,
                            void *ctx)
{
    const uint8_t valid = TB_BTN_1 | TB_BTN_2 | TB_BTN_3 | TB_BTN_4;
    uint8_t changed;

    prev &= valid;
    now &= valid;
    changed = (uint8_t)(prev ^ now);

    if (cb != NULL) {
        /*
         * Ascending index order, so two buttons changing in the same poll
         * arrive in a stable order rather than depending on bit tricks. The UI
         * treats each independently, but a deterministic order makes the
         * behaviour reproducible when debugging.
         */
        for (uint8_t i = 0; i < 4U; i++) {
            const uint8_t mask = (uint8_t)(1U << i);
            if ((changed & mask) != 0U) {
                cb(i, (now & mask) != 0U, ctx);
            }
        }
    }

    return now;
}
