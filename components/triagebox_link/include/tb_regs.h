#ifndef TB_REGS_H
#define TB_REGS_H

#include <stdint.h>

/*
 * I2C register map: ESP32-S3 (master) <-> STM32F411 (slave).
 *
 * ONE SOURCE OF TRUTH, TWO COPIES. This file is platform-neutral on purpose --
 * no HAL, no ESP-IDF, no malloc. The ESP32 repo gets a verbatim copy at
 * components/triagebox_link/include/tb_regs.h. If you edit one, copy it over;
 * TB_PROTO_VER exists so a stale copy fails loudly instead of silently
 * misreading bytes.
 *
 * WHY A REGISTER MAP AND NOT tb_frame: I2C already provides what tb_frame's
 * 0xA5/0x5A sync bytes and CRC were built for on RS485 -- start/stop delimit
 * every transaction and each byte is ACKed by hardware. Re-framing inside that
 * would be belt-and-braces on a 10 cm trace. The payoff is debuggability: this
 * looks like any other I2C chip, so the ESP32's existing read-only console
 * commands (`i2creg`, `i2cdump`, `i2craw`) can inspect the STM32 with no new
 * tooling on either side. tb_frame stays in the ESP32 tree for the LoRa path.
 *
 * TRANSACTION SHAPE (standard "write pointer, then read"):
 *
 *   read : S 0x42 W [reg] Sr 0x42 R [d0] [d1] ... P
 *   write: S 0x42 W [reg] [d0] [d1] ... P
 *
 * The read block is a SNAPSHOT: the slave latches a consistent copy when the
 * master addresses it for reading, so a multi-byte read can never mix an old
 * heart rate with a new SpO2. The pointer auto-increments within the block, so
 * the whole thing is one transaction. (Contrast the TCA9554 and SW6106 on this
 * same bus, which do NOT auto-increment -- reading 4 bytes there returns the
 * same register 4 times.)
 *
 * All multi-byte values are LITTLE-ENDIAN, matching both MCUs' native order
 * and tb_frame, so both sides can memcpy rather than shift.
 */

/*
 * 7-bit slave address. Free on this bus: the V3.0 scan found 0x20 (TCA9554),
 * 0x3c (SW6106 PMIC), 0x51 (PCF85063A RTC), 0x5d (GT911 touch), plus a
 * partial-decode phantom at 0x26. Do not reuse any of those.
 *
 * Note the STM32 HAL wants this SHIFTED LEFT BY 1 in hi2c.Init.OwnAddress1;
 * the ESP-IDF master API wants it unshifted. TB_I2C_SLAVE_ADDR_HAL does that
 * conversion once so the shift is never open-coded.
 */
#define TB_I2C_SLAVE_ADDR      0x42U
#define TB_I2C_SLAVE_ADDR_HAL  (TB_I2C_SLAVE_ADDR << 1)

/* Bump on ANY layout change. The ESP32 reads this first and refuses to parse
 * a block it does not recognise, rather than trusting shifted offsets. */
#define TB_PROTO_VER 0x01U

/* ---- Read block (STM32 -> ESP32) ---------------------------------------- */

#define TB_REG_PROTO_VER  0x00U /* u8   always TB_PROTO_VER */
#define TB_REG_SEQ        0x01U /* u8   ++ per publish, wraps; see below */
#define TB_REG_FLAGS      0x02U /* u8   TB_FLAG_* */
#define TB_REG_BUTTONS    0x03U /* u8   TB_BTN_* state, 1 = pressed */
#define TB_REG_HR         0x04U /* u16  bpm, 0 = no reading */
#define TB_REG_SPO2       0x06U /* u16  %, 0 = no reading */
#define TB_REG_RR_X10     0x08U /* u16  breaths/min * 10, 0 = no reading */
#define TB_REG_BP_SYS     0x0AU /* u16  mmHg, valid only if TB_FLAG_BP_VALID */
#define TB_REG_BP_DIA     0x0CU /* u16  mmHg, valid only if TB_FLAG_BP_VALID */
#define TB_REG_BATTERY    0x0EU /* u8   percent, 0xFF = not measured */
#define TB_REG_SENSOR_OK  0x0FU /* u8   TB_SENSOR_* */
#define TB_REG_RFID_LEN   0x10U /* u8   bytes of ASCII tag below, 0 = no tag */
#define TB_REG_RFID       0x11U /* char[TB_RFID_MAX], NOT NUL-terminated */

#define TB_RFID_MAX       31U   /* matches RFID_TAG_CAPACITY-1 on the ESP32 */
#define TB_REG_READ_END   (TB_REG_RFID + TB_RFID_MAX) /* 0x30, one past end */

/*
 * SEQ is how the ESP32 tells "sensors quiet" from "STM32 dead": it increments
 * on every publish even when no reading changed, so a frozen SEQ across
 * several polls means the superloop stalled. Wrapping at 256 is fine -- the
 * ESP32 only compares for inequality, never for ordering.
 */

/* ---- Write block (ESP32 -> STM32) --------------------------------------- */

#define TB_REG_CMD        0x40U /* u8   TB_CMD_*, self-clearing once acted on */
#define TB_REG_PRIORITY   0x41U /* u8   LoRa order: 0=BLACK 1=RED 2=YELLOW 3=GREEN */
#define TB_REG_CONFIDENCE 0x42U /* u8   0..100 */
#define TB_REG_WRITE_END  0x43U

/*
 * PRIORITY uses the LoRa numeric alias, NOT ui_priority_t's declaration order
 * (RED, YELLOW, GREEN, BLACK). The ESP32 must convert with
 * tb_frame_priority_to_wire() before writing here. Getting this wrong swaps
 * RED and BLACK -- the two that matter most -- so it fails silently and badly.
 */

/* Commands. Same numbering as tb_cmd_t in the ESP32's tb_frame.h. */
#define TB_CMD_NONE          0x00U
#define TB_CMD_START_SCAN    0x01U
#define TB_CMD_START_MEASURE 0x02U
#define TB_CMD_ABORT         0x03U
#define TB_CMD_POWER_OFF     0x04U

/* ---- Bit definitions ---------------------------------------------------- */

/*
 * Buttons are a STATE bitmask, not an event. Idempotent by design: a poll the
 * ESP32 misses costs nothing and there is no queue here to overflow.
 *
 * The pins are input + pull-up, so a pressed button reads 0 at the GPIO. That
 * inversion is the STM32's business and is undone before publishing -- on the
 * wire 1 ALWAYS means pressed. Do not let the pull direction leak across the
 * link; the ESP32 has no way to verify it.
 *
 * The ESP32 turns this into edges itself (XOR against the previous byte), so
 * debounce stays here, where the pins are.
 */
#define TB_BTN_1 0x01U /* PB12 */
#define TB_BTN_2 0x02U /* PB13 */
#define TB_BTN_3 0x04U /* PB14 */
#define TB_BTN_4 0x08U /* PB15 */

/* Vitals freshness. HR/SpO2/RR each have their own bit because they come from
 * different sensors on different cadences -- a finger off the MAX30102 must
 * not invalidate a good ECG heart rate. */
#define TB_FLAG_HR_VALID   0x01U
#define TB_FLAG_SPO2_VALID 0x02U
#define TB_FLAG_RR_VALID   0x04U
/*
 * BP has no source yet: the MPX5010 was the old respiratory method and PA2 is
 * now the breathing microphone, so nothing measures pressure. This bit stays 0
 * and BP_SYS/BP_DIA stay 0 until a BP method is chosen. It is a separate bit
 * rather than "0 means absent" so the ESP32's SVM can tell a missing feature
 * from a genuine reading and refuse to score instead of inventing one -- BP is
 * 2 of its 5 features.
 */
#define TB_FLAG_BP_VALID   0x08U
#define TB_FLAG_MEASURING  0x10U /* a measure window is running */

/* Per-sensor health, for the ESP32's Home status dots. */
#define TB_SENSOR_ECG      0x01U
#define TB_SENSOR_MAX30102 0x02U
#define TB_SENSOR_MIC      0x04U
#define TB_SENSOR_RFID     0x08U /* PN532; not in the build yet, so always 0 */
#define TB_SENSOR_LORA     0x10U

/*
 * The read block, laid out to match the offsets above exactly. Packed because
 * it IS the wire image: the slave hands a pointer into this straight to the
 * I2C peripheral, with no per-field serialisation step to get wrong.
 * tb_regs_selftest.c pins every offset with offsetof().
 */
#if defined(__GNUC__)
#define TB_PACKED __attribute__((packed))
#else
#define TB_PACKED
#endif

typedef struct TB_PACKED {
    uint8_t  proto_ver;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  buttons;
    uint16_t hr;
    uint16_t spo2;
    uint16_t rr_x10;
    uint16_t bp_sys;
    uint16_t bp_dia;
    uint8_t  battery;
    uint8_t  sensor_ok;
    uint8_t  rfid_len;
    char     rfid[TB_RFID_MAX];
} tb_snapshot_t;

#endif /* TB_REGS_H */
