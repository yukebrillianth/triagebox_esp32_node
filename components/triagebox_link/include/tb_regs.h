#ifndef TB_REGS_H
#define TB_REGS_H

#include <stddef.h> /* NULL, for tb_wave_take's optional out-parameter */
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
 * 0x02 added the PPG waveform block at TB_REG_PPG_BASE.
 * 0x03 put ECG into that block alongside IR/RED (so a sample is 6 bytes, not 4)
 *      and shrank the ring to 20 samples to keep the block inside the one-byte
 *      register pointer. Both the stride and the count moved, so every offset
 *      past the first sample moved with them -- an 0x02 reader parsing an 0x03
 *      block would find RED where IR is and drift by two bytes per sample. This
 *      is exactly the case the version byte exists to stop. */
#define TB_PROTO_VER 0x03U

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
#define TB_REG_READ_END   (TB_REG_RFID + TB_RFID_MAX) /* 0x30, one past the vitals */

/*
 * Downlink RSSI: how strongly THIS node heard the station's last poll, in dBm.
 *
 * This is the only link-quality number the node can possibly know. The node
 * never transmits unpolled (see lora_poll.h), so its radio sits in RX continuous
 * and hears every poll -- and RegPktRssiValue after RxDone is a real measurement
 * taken at this node's position. It is the STATION->NODE direction, not the
 * reverse, but path loss is reciprocal, so for walking the box away from the
 * station to find the range it is exactly the right number.
 *
 * The uplink RSSI (what the station heard) is measured by the station's own radio
 * and published in its node status JSON. That one cannot reach this screen
 * without a second downlink field, and it would tell you the same thing.
 *
 * int8_t dBm, so -128..-2 covers everything a real LoRa receiver reports:
 * SF7/125k sensitivity is about -123 dBm at the weak end, and the strong end is
 * bounded by the two sentinels below rather than by physics. With an antenna
 * fitted and the station on the same desk this measured -12. tb_rssi_valid()
 * below is what a consumer must use rather than testing for a sentinel, because
 * there are two different "no reading" values in play (see it for why).
 *
 * DELIBERATELY OUTSIDE TB_REG_READ_END. The 50 ms vitals poll still reads
 * exactly 0x30 bytes, so an STM32 built before this field existed answers that
 * poll unchanged -- no version bump, no lockstep reflash. The ESP32 picks this
 * byte up in its own 1-byte read once a second, and an old slave answers that
 * read with its 0xFF out-of-range pad, which tb_rssi_valid() rejects. Appending
 * it INSIDE the block instead would have made the master ask for one byte more
 * than an old slave's buffer holds, which is a failed vitals poll -- i.e. a link
 * that looks dead -- in exchange for saving one byte a second.
 */
#define TB_REG_LORA_RSSI  0x30U /* i8   dBm; see tb_rssi_valid() */

/*
 * The whole readable image, which is one byte longer than the vitals block.
 * The STM32's slave sizes its staging buffer from sizeof(tb_snapshot_t), so this
 * is what makes 0x30 readable at all.
 */
#define TB_REG_SNAPSHOT_END (TB_REG_LORA_RSSI + 1U) /* 0x31 */

/*
 * Two values mean "no reading", from two different places, which is why this is
 * a range test and not `!= SENTINEL`:
 *
 *   0x00  a new STM32 that has not received a poll yet (the field is zeroed at
 *         boot, and 0 dBm is not a level any receiver reports)
 *   0xFF  an old STM32, or any address it does not decode: its AddrCallback
 *         feeds 0xFF rather than leaking adjacent memory
 *
 * They are adjacent (-1 and 0), so a single upper bound at -2 rejects both and
 * no special case is needed.
 *
 * THAT BOUND IS SET BY THE SENTINELS, NOT BY SATURATION. It was -20 until
 * 2026-08-29, on the theory that anything stronger had to be front-end overload
 * at arm's length. The first range test with an antenna fitted read -12, which
 * is a true measurement, and the status bar dropped back to "LoRa siap" and
 * stayed there -- tb_ui_source_on_rssi() never latches a rejected value, so a
 * link that was working looked like a link with no radio. A guard must reject
 * only what cannot be true; how strong a real reading gets is not its business.
 *
 * Only the upper bound is tested. TB_RSSI_MIN_DBM is exactly INT8_MIN, so an
 * int8_t cannot carry anything below it -- the type is the lower bound, and
 * writing the comparison anyway is what -Werror=type-limits rejects. It stays
 * defined because it documents the field's range and because tb_rssi_valid()
 * would need to grow that test back if the field ever widened.
 */
#define TB_RSSI_MIN_DBM (-128) /* == INT8_MIN: structural, not checked below */
#define TB_RSSI_MAX_DBM (-2)   /* one below the 0xFF sentinel, not a signal limit */

static inline int tb_rssi_valid(int8_t rssi)
{
    return rssi <= TB_RSSI_MAX_DBM;
}

/*
 * SEQ is how the ESP32 tells "sensors quiet" from "STM32 dead": it increments
 * on every publish even when no reading changed, so a frozen SEQ across
 * several polls means the superloop stalled. Wrapping at 256 is fine -- the
 * ESP32 only compares for inequality, never for ordering.
 */

/* ---- Waveform block (STM32 -> ESP32) ------------------------------------ */

/*
 * The three sampled signals the ESP32 needs to do its own processing: the
 * smoothed MAX30102 IR and RED channels and the boxcar-averaged ECG, one
 * time-aligned triple per sample slot. The STM32 does not analyse any of it
 * beyond SpO2 and heart rate; it low-passes the PPG (31-tap linear-phase FIR,
 * 10Hz at 100Hz -- see DSP_PPG_LP_TAPS) and hands all three over.
 *
 * WHY ONE BLOCK AND NOT TWO: the ESP32's blood-pressure model is fed pulse
 * transit time -- the delay between the R wave on the ECG and the foot of the
 * pulse at the finger -- so its inputs are only as good as the alignment
 * between the two channels. One struct with one sample counter makes that
 * alignment structural. Two blocks with two counters would need the ESP32 to
 * correlate them, and any read that caught the two at different heads would
 * shift PTT by whole samples, i.e. by tens of mmHg of predicted pressure.
 *
 * WHY A SEPARATE BASE ADDRESS AND NOT AN EXTENSION OF tb_snapshot_t: I2C2 runs
 * at 100kHz, so a byte costs ~90us. Appending 124 bytes of waveform to the
 * vitals snapshot would turn a 4ms poll into a 15ms one, on a bus the ESP32
 * also shares with the GT911 touch controller and the TCA9554. Two base
 * addresses means the ESP32 polls vitals as often as it likes and pulls the
 * waveform only when something wants it.
 *
 * WHY A RING WITH A TOTAL AND NO POP: a slave cannot tell how many bytes the
 * master actually took -- a read ends when the master NACKs, which the F4 HAL
 * surfaces as an error, not a count. So nothing is consumed here. The ESP32
 * diffs `total` against the value it saw last time and reads that many samples
 * backwards from the head; tb_wave_take() below does exactly that, so neither
 * side has to re-derive the wrap. Same reasoning as the button bitmask: state,
 * not events, so a missed poll costs nothing and there is no queue to overflow.
 *
 * The overrun is detectable rather than silent, which is the point: at
 * TB_PPG_FS_HZ the ring holds TB_PPG_RING/TB_PPG_FS_HZ = 200ms, so the ESP32
 * must read at better than ~5Hz. Slower than that is not corruption, it is a
 * gap tb_wave_take() reports.
 */

/*
 * 0x50 still, and the ceiling is still the ONE-BYTE register pointer: the block
 * must end at or before 0x100, which at 6 bytes a sample plus the 4-byte counter
 * leaves room for at most (0x100 - 0x50 - 4) / 6 = 28 samples from here.
 *
 * It cannot move down -- 0x50 already sits clear of the read block (ends 0x31)
 * and the write block (0x40..0x47), and the ESP32's codec asserts
 * TB_REG_SNAPSHOT_END <= TB_REG_PPG_BASE. Moving it up only costs samples.
 */
#define TB_REG_PPG_BASE 0x50U
/*
 * NOT a power of two any more, and that is the 6-byte sample talking: 32 samples
 * would be 196 bytes and end at 0x114, past the one-byte pointer, so the ring had
 * to shrink and the obvious replacement -- 16, to keep the wrap a mask -- is only
 * 160 ms of history. 20 samples is 200 ms at 100 Hz, so the index wrap below is a
 * divide rather than a mask. On a Cortex-M4 that is a single UDIV once per push at
 * 100 Hz, which is not measurable.
 *
 * 20 and not the 28 the address space would allow, because the number that
 * matters is the poll budget, not the maximum: the ESP32 polls the snapshot every
 * 50 ms, so 200 ms is 4 reads per turnover -- it may miss three polls in a row and
 * still lose no sample. 124 bytes is ~11 ms of a 100 kHz bus per read, and the
 * bigger ring would spend 30% more bus time on history nothing asks for.
 */
#define TB_PPG_RING     20U
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

/*
 * ONE SAMPLE INSTANT, THREE SIGNALS. ir/red are MAX30102 counts >> TB_PPG_SHIFT
 * (above); ecg is NOT shifted and NOT packed -- it is the raw ADC word.
 *
 * The asymmetry is the two sensors' resolutions, not an oversight. The MAX30102
 * FIFO is 18-bit, so it needs the shift to fit a uint16_t at all. The F411's ADC
 * is 12-bit, so an ECG sample fits with four bits to spare and shifting it would
 * throw away resolution that is already scarce: the AD8232 delivers an R wave of
 * a few hundred counts on a ~2000-count baseline, and >>2 would quantise the R
 * peak the ESP32 has to time the pulse transit from.
 */
typedef struct TB_PACKED {
    uint16_t ir;  /**< tb_ppg_unpack() -> raw counts */
    uint16_t red;
    uint16_t ecg; /**< raw 12-bit ADC counts, use as-is */
} tb_wave_sample_t;

/*
 * `total` deliberately carries the head position too: head == total %
 * TB_PPG_RING. One field cannot disagree with itself, whereas a separate head
 * index could be latched out of step with the counter it describes.
 *
 * u32 at offset 0 keeps every uint16_t below it naturally aligned, so both
 * sides can cast this over the received bytes instead of unpacking by hand.
 * It wraps after 497 days at 100Hz; the ESP32 subtracts, so the wrap is
 * harmless as long as it uses uint32_t arithmetic.
 *
 * The modulo survives that wrap because both sides apply `% TB_PPG_RING` to the
 * same uint32_t counter values, so the reader derives exactly the slots the writer
 * used -- tb_link_selftest.c pins that. The one imperfection: 2^32 % 20 == 16, so
 * across the rollover slot 0 is reused after 16 samples rather than 20, and a read
 * that is 17-20 samples behind at exactly that instant gets one slot the writer
 * had already overwritten, with no drop reported. One sample, in one read, once per
 * 497 days of uptime -- against a mask, which would have cost 4 samples of history
 * on every read forever.
 */
typedef struct TB_PACKED {
    uint32_t total;                     /**< samples ever pushed, never reset */
    tb_wave_sample_t s[TB_PPG_RING];
} tb_wave_block_t;

#define TB_REG_PPG_END (TB_REG_PPG_BASE + 0x7CU) /* 0xCC, one past end */

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
 * Append one sample: PPG in raw counts, ECG in raw ADC counts. STM32 side;
 * wrapped by tb_slave_wave_push(). @p blk is volatile because the I2C ISR reads
 * it concurrently, and that is load-bearing:
 *
 * total is stored LAST, and that is the whole synchronisation scheme -- there
 * is no lock and none is needed. The reader treats total as "samples you may
 * trust", so publishing it after the sample it describes means an interrupt
 * landing mid-push sees either the old count, and picks this sample up next
 * time, or the new count with the sample already in place. Never a count that
 * promises a slot not yet written. The volatile qualifier is what stops the
 * compiler hoisting that store above the three below; on Cortex-M4 that is
 * sufficient, since the core sees its own stores in program order.
 */
static inline void tb_wave_push(volatile tb_wave_block_t *blk, float ir,
                                float red, uint16_t ecg)
{
    uint32_t t = blk->total;
    uint32_t i = t % TB_PPG_RING; /* not a power of two, so a divide */

    blk->s[i].ir = tb_ppg_pack(ir);
    blk->s[i].red = tb_ppg_pack(red);
    blk->s[i].ecg = ecg;
    blk->total = t + 1U; /* last, deliberately */
}

/**
 * ESP32 side: pull everything new out of a block just read over I2C.
 *
 * @param blk         the 124 bytes read from TB_REG_PPG_BASE
 * @param last_total  in/out; keep it between calls, start it at 0
 * @param out         receives up to TB_PPG_RING samples, OLDEST FIRST
 * @param dropped     may be NULL; else set to the count lost to the ring
 *                    turning over, i.e. this read came too late
 * @return            how many samples were written to @p out
 *
 * ir/red still hold wire values -- run tb_ppg_unpack() for counts. ecg is
 * already raw ADC counts and must not be unpacked.
 */
static inline uint32_t tb_wave_take(const tb_wave_block_t *blk,
                                    uint32_t *last_total,
                                    tb_wave_sample_t *out, uint32_t *dropped)
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
        out[i] = blk->s[(total - n + i) % TB_PPG_RING];
    }
    *last_total = total;
    return n;
}

/* ---- Write block (ESP32 -> STM32) --------------------------------------- */

#define TB_REG_CMD          0x40U /* u8   TB_CMD_*, self-clearing once acted on */
#define TB_REG_PRIORITY     0x41U /* u8   LoRa order: 0=BLACK 1=RED 2=YELLOW 3=GREEN */
#define TB_REG_CONFIDENCE   0x42U /* u8   0..100 */
#define TB_REG_HOST_BATTERY 0x43U /* u8   percent, 0xFF = ESP32 has no reading */
#define TB_REG_HOST_BP_SYS  0x44U /* u16  mmHg, ESP32's ML prediction */
#define TB_REG_HOST_BP_DIA  0x46U /* u16  mmHg; its LAST byte latches the pair */
#define TB_REG_WRITE_END    0x48U

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

/*
 * HOST_BP_SYS / HOST_BP_DIA: the blood pressure the ESP32's model PREDICTED,
 * travelling backwards across this link for the same reason HOST_BATTERY does --
 * the board that computes it is not the board that transmits it.
 *
 * The STM32 does not compute blood pressure and there is no cuff on this node.
 * The ESP32 runs the ML model over the waveform block above (pulse transit time
 * between the ECG R wave and the finger pulse, plus PPG morphology) and writes
 * the result here in mmHg; the STM32's only job is to stamp the pair into the
 * LoRa packet and echo it in the snapshot, so the station and the ESP32 report
 * the same number rather than two independently-derived ones.
 *
 * HOST_ IN THE NAME IS NOT DECORATION. TB_REG_BP_SYS (0x0A) and TB_REG_BP_DIA
 * (0x0C) already exist in the READ block and are what every consumer reads;
 * these are the write-side inlet for the same value, exactly as HOST_BATTERY
 * (0x43) is the inlet for BATTERY (0x0E). Two registers, two directions, one
 * value -- and they must not be the same address or the ESP32's write would land
 * on top of what it is about to read.
 *
 * THE PAIR LATCHES ON THE LAST BYTE OF DIA, mirroring the "confidence is written
 * last" rule at TB_REG_CONFIDENCE, and for the same reason: the register pointer
 * auto-increments, so one transaction starting at 0x44 delivers sys-lo, sys-hi,
 * dia-lo, dia-hi in that order, and only after the fourth byte is there a pair
 * to believe. Latching earlier would publish a systolic from this model run
 * beside a diastolic from the last one. The ESP32 must therefore write all four
 * bytes in one transaction, DIA last -- writing DIA alone latches it against a
 * stale systolic.
 *
 * The STM32 validates before latching (tb_bp_pair_valid below) and discards BOTH
 * values on a failure, counting it in mon_bp_writes_rejected. This is not
 * defensive tidiness: an ML model asked to extrapolate outside its training set
 * returns a number, not an error, and a garbage 300/250 stamped into a LoRa
 * packet becomes a triage input at the station with no way back to its source.
 * A rejected pair leaves the previous good one standing, so the failure mode is
 * a stale reading rather than an invented one.
 */
#define TB_BP_SYS_MIN 40U  /* below this is not a perfusing patient */
#define TB_BP_SYS_MAX 260U /* above this is not a reading a cuff would give */
#define TB_BP_DIA_MIN 20U
#define TB_BP_DIA_MAX 180U

/**
 * Is this a pair worth putting on the wire? Range plus the one relation that
 * makes them a pair at all: diastolic is the pressure between beats, so
 * dia >= sys is not a hypertensive patient, it is a broken prediction.
 *
 * Strictly less, not <=: dia == sys means zero pulse pressure, which is cardiac
 * arrest with a pulse oximeter still reading -- physically incoherent rather
 * than merely alarming.
 *
 * In the header so the STM32's write path and the ESP32's model wrapper cannot
 * hold two different opinions about what a plausible reading is; returns int
 * rather than bool for the same reason tb_rssi_valid() does -- this header is
 * included in places that have not pulled in <stdbool.h>.
 */
static inline int tb_bp_pair_valid(uint16_t sys, uint16_t dia)
{
    return (sys >= TB_BP_SYS_MIN) && (sys <= TB_BP_SYS_MAX)
        && (dia >= TB_BP_DIA_MIN) && (dia <= TB_BP_DIA_MAX)
        && (dia < sys);
}

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
 * BP comes from the ESP32's ML model, not from a cuff on this node: it is
 * written back at TB_REG_HOST_BP_SYS/DIA and echoed in the read block. This bit
 * is set only while a validated pair is standing, so it is clear from boot until
 * the first prediction arrives, and clear again for any pair the range test
 * rejected.
 *
 * It is a separate bit rather than "0 means absent" because the ESP32's SVM uses
 * BP as 2 of its 5 features and must be able to tell a missing feature from a
 * genuine reading -- and it consumes its own prediction back through this block,
 * so the bit is also how it learns the STM32 accepted the write.
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
 * idle-but-answering sensor. It says nothing about the ECG channel in the same
 * block either: chest electrodes and a finger clip come off independently, so
 * that one is TB_SENSOR_ECG.
 *
 * Deliberately in this byte rather than in the waveform block: the snapshot is
 * polled far more often than the 124-byte waveform, so contact loss is visible
 * without paying for a waveform read that is going to be discarded.
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
#define TB_SENSOR_RFID     0x08U /* PN532; set once the module answers, card or not */
#define TB_SENSOR_LORA     0x10U

/*
 * The read block, laid out to match the offsets above exactly. Packed because
 * it IS the wire image: the slave hands a pointer into this straight to the
 * I2C peripheral, with no per-field serialisation step to get wrong.
 * tb_link_selftest.c pins every offset with offsetof().
 *
 * sizeof() is TB_REG_SNAPSHOT_END (0x31), one more than TB_REG_READ_END (0x30):
 * the slave sizes its staging buffer from this struct, so lora_rssi being a
 * member is exactly what makes register 0x30 readable. The vitals poll still
 * asks for 0x30 bytes and is unaffected -- see TB_REG_LORA_RSSI.
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
    int8_t   lora_rssi;
} tb_snapshot_t;

#endif /* TB_REGS_H */
