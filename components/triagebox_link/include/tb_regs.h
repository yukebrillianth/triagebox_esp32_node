#ifndef TB_REGS_H
#define TB_REGS_H

#include <stddef.h> /* NULL, for tb_ppg_take's optional out-parameter */
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

/* Every struct here is the wire image itself, so none of them may gain padding.
 * Defined up here because both block definitions below need it. */
#if defined(__GNUC__)
#define TB_PACKED __attribute__((packed))
#else
#define TB_PACKED
#endif

/* Bump on ANY layout change. The ESP32 reads this first and refuses to parse
 * a block it does not recognise, rather than trusting shifted offsets.
 * 0x02 added the PPG waveform block at TB_REG_PPG_BASE. */
#define TB_PROTO_VER 0x02U

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
#define TB_REG_BATTERY    0x0EU /* u8   percent, 0xFF = none; TB_REG_HOST_BATTERY */
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

/* ---- PPG waveform block (STM32 -> ESP32) -------------------------------- */

/*
 * The smoothed MAX30102 waveform, so the ESP32 can run its own PPG processing.
 * The STM32 does not analyse it beyond SpO2; it low-passes it (31-tap
 * linear-phase FIR, 10Hz at 100Hz -- see DSP_PPG_LP_TAPS) and hands it over.
 *
 * WHY A SEPARATE BASE ADDRESS AND NOT AN EXTENSION OF tb_snapshot_t: I2C2 runs
 * at 100kHz, so a byte costs ~90us. Appending 128 bytes of waveform to the
 * vitals snapshot would turn a 4ms poll into a 16ms one, on a bus the ESP32
 * also shares with the GT911 touch controller and the TCA9554. Two base
 * addresses means the ESP32 polls vitals as often as it likes and pulls the
 * waveform only when something wants it.
 *
 * WHY A RING WITH A TOTAL AND NO POP: a slave cannot tell how many bytes the
 * master actually took -- a read ends when the master NACKs, which the F4 HAL
 * surfaces as an error, not a count. So nothing is consumed here. The ESP32
 * diffs `total` against the value it saw last time and reads that many samples
 * backwards from the head; tb_ppg_take() below does exactly that, so neither
 * side has to re-derive the wrap. Same reasoning as the button bitmask: state,
 * not events, so a missed poll costs nothing and there is no queue to overflow.
 *
 * The overrun is detectable rather than silent, which is the point: at
 * TB_PPG_FS_HZ the ring holds TB_PPG_RING/TB_PPG_FS_HZ = 320ms, so the ESP32
 * must read at better than ~3Hz. Slower than that is not corruption, it is a
 * gap tb_ppg_take() reports.
 */

/* 0x50 and not 0x80: the register pointer is ONE byte, and this block is 132
 * bytes, so a base of 0x80 would run off the end of the address space at 0x104
 * and the tail would be unreachable. 0x50 sits clear of both the read block
 * (ends 0x30) and the write block (0x40..0x43) and ends at 0xD4. */
#define TB_REG_PPG_BASE 0x50U
/* Power of two so the index wrap is a mask, not a divide. 32 samples = 320ms
 * at 100Hz, and 128 bytes = ~12ms of a 100kHz bus per read. Raising it buys
 * slack for a slower poller at the cost of a longer transaction. */
#define TB_PPG_RING     32U
/* Sample rate of what is in the ring, from DSP_PPG_FS_HZ. A constant, not a
 * wire field: the ESP32 needs it to interpret the waveform, but it never
 * changes at runtime, so putting it on the bus 100 times a second would be
 * paying for a value that cannot vary. */
#define TB_PPG_FS_HZ    100U

/*
 * Samples are counts >> TB_PPG_SHIFT. The MAX30102 FIFO is 18-bit, so >>2 maps
 * its full 0..262143 range onto a uint16_t exactly (0x3FFFF >> 2 == 0xFFFF) and
 * halves the bus time against a float32. The 4-count quantisation step is well
 * under the ~10 counts of noise left after the smoothing FIR, on a pulse of
 * ~200 counts, so it costs nothing the filter had won.
 */
#define TB_PPG_SHIFT      2U
#define TB_PPG_MAX_COUNTS 0x3FFFFU /* 18-bit MAX30102 FIFO word */

typedef struct TB_PACKED {
    uint16_t ir;  /**< tb_ppg_unpack() -> raw counts */
    uint16_t red;
} tb_ppg_sample_t;

/*
 * `total` deliberately carries the head position too: head == total %
 * TB_PPG_RING. One field cannot disagree with itself, whereas a separate head
 * index could be latched out of step with the counter it describes.
 *
 * u32 at offset 0 keeps every uint16_t below it naturally aligned, so both
 * sides can cast this over the received bytes instead of unpacking by hand.
 * It wraps after 497 days at 100Hz; the ESP32 subtracts, so the wrap is
 * harmless as long as it uses uint32_t arithmetic.
 */
typedef struct TB_PACKED {
    uint32_t total;                     /**< samples ever pushed, never reset */
    tb_ppg_sample_t s[TB_PPG_RING];
} tb_ppg_block_t;

#define TB_REG_PPG_END (TB_REG_PPG_BASE + 0x84U) /* one past end */

/**
 * Pack one smoothed channel for the wire. Only the STM32 calls this; it lives
 * here so the shift, the clamp and tb_ppg_unpack() cannot drift apart.
 *
 * The `!(counts > 0)` test rather than `counts < 0` is deliberate: it also
 * catches NaN, and the FIR legitimately undershoots below zero for a few
 * samples after a step (its stopband taps are negative), which would otherwise
 * convert to a huge uint32_t.
 */
static inline uint16_t tb_ppg_pack(float counts)
{
    if (!(counts > 0.0f)) {
        return 0U;
    }
    if (counts >= (float)TB_PPG_MAX_COUNTS) {
        return (uint16_t)(TB_PPG_MAX_COUNTS >> TB_PPG_SHIFT);
    }
    return (uint16_t)((uint32_t)(counts + 0.5f) >> TB_PPG_SHIFT);
}

/** Wire value back to MAX30102 counts. This is the ESP32's half. */
static inline uint32_t tb_ppg_unpack(uint16_t v)
{
    return (uint32_t)v << TB_PPG_SHIFT;
}

/**
 * Append one sample, in raw counts. STM32 side; wrapped by
 * tb_slave_ppg_push(). @p blk is volatile because the I2C ISR reads it
 * concurrently, and that is load-bearing:
 *
 * total is stored LAST, and that is the whole synchronisation scheme -- there
 * is no lock and none is needed. The reader treats total as "samples you may
 * trust", so publishing it after the sample it describes means an interrupt
 * landing mid-push sees either the old count, and picks this sample up next
 * time, or the new count with the sample already in place. Never a count that
 * promises a slot not yet written. The volatile qualifier is what stops the
 * compiler hoisting that store above the two below; on Cortex-M4 that is
 * sufficient, since the core sees its own stores in program order.
 */
static inline void tb_ppg_push(volatile tb_ppg_block_t *blk, float ir, float red)
{
    uint32_t t = blk->total;
    uint32_t i = t & (TB_PPG_RING - 1U); /* power of two, so a mask */

    blk->s[i].ir = tb_ppg_pack(ir);
    blk->s[i].red = tb_ppg_pack(red);
    blk->total = t + 1U; /* last, deliberately */
}

/**
 * ESP32 side: pull everything new out of a block just read over I2C.
 *
 * @param blk         the 132 bytes read from TB_REG_PPG_BASE
 * @param last_total  in/out; keep it between calls, start it at 0
 * @param out         receives up to TB_PPG_RING samples, OLDEST FIRST
 * @param dropped     may be NULL; else set to the count lost to the ring
 *                    turning over, i.e. this read came too late
 * @return            how many samples were written to @p out
 *
 * Samples still hold wire values -- run tb_ppg_unpack() for counts.
 */
static inline uint32_t tb_ppg_take(const tb_ppg_block_t *blk,
                                   uint32_t *last_total,
                                   tb_ppg_sample_t *out, uint32_t *dropped)
{
    uint32_t total = blk->total;
    uint32_t n = total - *last_total; /* u32 subtraction, so the wrap is fine */
    uint32_t i;

    if (dropped != NULL) {
        *dropped = (n > TB_PPG_RING) ? (n - TB_PPG_RING) : 0U;
    }
    if (n > TB_PPG_RING) {
        n = TB_PPG_RING; /* the rest are already overwritten */
    }
    for (i = 0U; i < n; ++i) {
        out[i] = blk->s[(total - n + i) & (TB_PPG_RING - 1U)];
    }
    *last_total = total;
    return n;
}

/* ---- Write block (ESP32 -> STM32) --------------------------------------- */

#define TB_REG_CMD          0x40U /* u8   TB_CMD_*, self-clearing once acted on */
#define TB_REG_PRIORITY     0x41U /* u8   LoRa order: 0=BLACK 1=RED 2=YELLOW 3=GREEN */
#define TB_REG_CONFIDENCE   0x42U /* u8   0..100 */
#define TB_REG_HOST_BATTERY 0x43U /* u8   percent, 0xFF = ESP32 has no reading */
#define TB_REG_WRITE_END    0x44U

/*
 * PRIORITY uses the LoRa numeric alias, NOT ui_priority_t's declaration order
 * (RED, YELLOW, GREEN, BLACK). The ESP32 must convert with
 * tb_frame_priority_to_wire() before writing here. Getting this wrong swaps
 * RED and BLACK -- the two that matter most -- so it fails silently and badly.
 */

/*
 * HOST_BATTERY travels backwards compared to everything else on this link, and
 * that is the whole reason it exists. The fuel gauge is the SW6106 PMIC at
 * 0x3c -- on this same bus, but as another SLAVE, so only the ESP32 (the master)
 * can read it. The LoRa packet is built here, on the STM32. So the board that
 * knows the percentage is not the board that has to transmit it, and one byte
 * across this link is the fix; the alternative was making the STM32 a second bus
 * master to reach 0x3c, on a bus the ESP32 polls every 50ms.
 *
 * TB_REG_BATTERY (0x0E) keeps its own meaning: the STM32's OWN measurement,
 * which this board does not have and reports as 0xFF. main.c substitutes
 * HOST_BATTERY into both the snapshot and the LoRa packet, so every consumer
 * still sees exactly one battery field and needs no fallback rule.
 *
 * 0xFF means "the ESP32 could not read the gauge", NOT 0%. The ESP32 writes 0xFF
 * on a failed PMIC read rather than holding the last good value, matching what
 * its own status bar does and for the same reason: a frozen 80% while the pack
 * drains is more dangerous than a blank cell for one cycle. Never substitute 0 --
 * 0 is a flat pack, and the station publishes 0 instead of omitting the key.
 *
 * WHY THIS DID NOT BUMP TB_PROTO_VER: nothing in the READ block moved, and the
 * read block is the only thing a version check protects -- same argument as
 * TB_FLAG_HR_FROM_PPG below. An STM32 built before this register existed hits
 * `default: break` in its write switch and ignores the byte, so it keeps
 * reporting 0xFF and the station keeps omitting the key: today's behaviour,
 * exactly. The two boards can be flashed independently, in either order.
 */

/* Commands. This is the single definition -- the ESP32's tb_frame.h used to
 * carry an equivalent tb_cmd_t enum and no longer does, because macros and an
 * enum with the same names cannot coexist in one translation unit. */
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
/*
 * A finger is on the MAX30102: the smoothed RED channel is at or above
 * DSP_PPG_MIN_DC. This is about the WAVEFORM at TB_REG_PPG_BASE, not about the
 * vitals -- TB_FLAG_SPO2_VALID already covers the number.
 *
 * Needed because the ring keeps advancing with nothing on the sensor: the STM32
 * pushes every sample unconditionally, so `total` climbing proves the sample
 * path is alive and says nothing about whether a finger is there. With nothing
 * on it the MAX30102 still reads ~2000 counts of ambient light and detector
 * noise, and PPG processing on the ESP32 side would find a heart rate in it.
 *
 * Clear means: draw the trace as no-contact, or draw nothing. It does NOT mean
 * the sensor is broken -- that is TB_SENSOR_MAX30102, which stays set for an
 * idle-but-answering sensor.
 *
 * Deliberately in this byte rather than in the PPG block: the snapshot is polled
 * far more often than the 132-byte waveform, so contact loss is visible without
 * paying for a waveform read that is going to be discarded.
 *
 * NOT part of the UI_VITAL_* mapping in tb_i2c_codec.c -- it gates the waveform,
 * not any displayed number.
 */
#define TB_FLAG_PPG_CONTACT 0x20U
/*
 * TB_REG_HR carried a PULSE rate from the PPG, not a heart rate from the ECG.
 * Set whenever the ECG produced nothing and the MAX30102 did, which for a
 * patient with a working finger sensor and no chest electrodes is the normal
 * case, not an error.
 *
 * WHY THE FIELD IS NOT RENAMED: TB_REG_HR keeps its name, offset and meaning
 * ("the patient's rate in bpm"), and the ESP32's UI_VITAL_HR mapping and the
 * station's MQTT "hr" key keep working untouched. A second rate register would
 * mean every consumer needs a fallback rule, and three copies of that rule
 * eventually disagree. So the fallback lives on the STM32 -- see mon_rate_bpm
 * in main.c -- and this bit reports which sensor won.
 *
 * WHY THIS DID NOT BUMP TB_PROTO_VER: nothing moved. A new bit in an existing
 * byte is layout-compatible, so an ESP32 built before this bit existed masks it
 * off and behaves exactly as it did. Bumping would have forced both boards to
 * be reflashed together to fix a label.
 *
 * What a consumer SHOULD do with it: label the number "PR" rather than "HR".
 * What it must not do is treat the reading as a heart rate for anything
 * rhythm-related. PR counts pulses that reached the finger, HR counts
 * depolarisations at the chest; a beat too weak to open the aortic valve is in
 * one and not the other, so PR <= HR always. For rate-based triage scoring the
 * two are interchangeable within a beat or two on a perfusing patient, which is
 * why this is a label bit and not a validity bit.
 */
#define TB_FLAG_HR_FROM_PPG 0x40U

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
 * tb_link_selftest.c pins every offset with offsetof().
 */
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
