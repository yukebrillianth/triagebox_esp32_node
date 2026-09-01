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

/* Diagnostics for the `stats` console command. */
uint32_t tb_link_frames_ok(void);   /* successful snapshot polls */
uint32_t tb_link_crc_errors(void);  /* failed polls + version rejects */

#endif /* TB_LINK_I2C_H */
