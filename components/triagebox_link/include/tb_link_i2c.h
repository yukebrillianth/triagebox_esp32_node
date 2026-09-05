#ifndef TB_LINK_I2C_H
#define TB_LINK_I2C_H

#include "esp_err.h"

#include "tb_regs.h"
#include "ui_types.h"

/*
 * I2C master link to the STM32F411 slave. Replaces the RS485/UART transport in
 * tb_link.c: chosen in CMake, never by #ifdef, so exactly one of the two is
 * linked. Wire contract: tb_regs.h (kept byte-identical with the STM32 copy).
 *
 * Why polled and not interrupt-driven: the STM32 has no spare pin wired as a
 * data-ready line, and the UI only redraws at 20 Hz anyway. A 50 ms poll is
 * well inside the perceptible-latency budget for a button press and costs about
 * 5 ms of bus time per second at 100 kHz.
 *
 * Shares the BSP I2C bus (SDA GPIO15 / SCL GPIO7) with the touch panel, the
 * TCA9554 expander, the RTC and the SW6106 PMIC. The IDF master driver
 * serialises single transactions per bus, but a register-pointer write and its
 * following read must stay one sequence, so every user of this bus takes the
 * shared bsp_i2c_lock() -- see tb_link_i2c.c.
 */

/*
 * Add the STM32 as a device on the BSP bus and start the poll task.
 * Safe to call with no STM32 attached: polls fail, the Home status dots show
 * the link down, and the UI still runs.
 *
 * Must be called AFTER the display brings up the I2C bus (bsp_i2c_get_handle()
 * must be valid) and BEFORE ui_runtime_init().
 */
esp_err_t tb_link_start(void);

/* Send one command (TB_CMD_* from tb_regs.h). Thread-safe. */
esp_err_t tb_link_send_cmd(uint8_t cmd);

/*
 * Hand the inference result to the STM32 for the LoRa TX. `confidence` is 0..1
 * and is quantized to 0..100. `tag` is accepted for signature compatibility
 * with the RS485 transport but is NOT sent: the STM32 already knows the tag (it
 * read it), so echoing 31 bytes back every measurement would be pure bus
 * traffic. Pass NULL.
 */
esp_err_t tb_link_send_result(ui_priority_t priority, float confidence,
                              const char *tag);

/*
 * The other thing a result can be: NO VERDICT.
 *
 * Writes TB_PRIORITY_WIRE_NONE (0xFF) to TB_REG_PRIORITY instead of a colour, so
 * the station omits the priority key and withholds the whole vital rather than
 * publishing it. Use it whenever the number on screen was not scored from this
 * patient's vitals -- the model refusing (esi 0), or demo mode. Reporting those
 * through tb_link_send_result() means the wire's BLACK, and BLACK is EXPECTANT.
 *
 * Separate from the call above rather than a flag on it because the caller's
 * question is different: "what colour did the model say" versus "there is no
 * verdict to send". A bool would let one be passed where the other was meant.
 */
esp_err_t tb_link_send_unscored(void);

/*
 * Hand the BP prediction (mmHg) to the STM32 so the LoRa packet carries it.
 * DIA's final byte latches the pair, so call only with a COMPLETE result;
 * an invalid/absent BP is simply not sent, and the STM32 keeps
 * TB_FLAG_BP_VALID clear so the station omits the keys.
 */
esp_err_t tb_link_send_bp(uint16_t sys, uint16_t dia);

/*
 * The four bytes only this board knows and the LoRa packet should carry:
 * rr_brpm (whole breaths/min, 0 = none), age_years, gender_ascii ('M'/'F',
 * 0 = not asked) and esi (the model's raw 1..5, 0 = it refused). Wire layout and
 * semantics: TB_REG_HOST_RR..TB_REG_HOST_ESI in tb_regs.h.
 *
 * MUST land before tb_link_send_result()/tb_link_send_unscored(): the slave
 * treats the verdict as complete once CONFIDENCE is written, and ESI belongs to
 * that verdict.
 */
esp_err_t tb_link_send_patient(uint8_t rr_brpm, uint8_t age_years,
                               uint8_t gender_ascii, uint8_t esi);

/*
 * Read the STM32F411's 96-bit factory UID into @p uid (TB_REG_UID_LEN bytes,
 * little-endian words). For the `uid` console command: it feeds the
 * UID->node_id table on the station, and a board must be addable to that table
 * from a serial monitor, without a debugger. Twelve 0xFF bytes back means the
 * slave predates the register -- the caller says so.
 */
esp_err_t tb_link_read_uid(uint8_t *uid);

/* Diagnostics for the `stats` console command. */
uint32_t tb_link_frames_ok(void);   /* successful snapshot polls */
uint32_t tb_link_crc_errors(void);  /* failed polls + version rejects */

#endif /* TB_LINK_I2C_H */
