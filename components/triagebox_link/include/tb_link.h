#ifndef TB_LINK_H
#define TB_LINK_H

#include "esp_err.h"

#include "tb_frame.h"
#include "ui_types.h"

/*
 * RS485 link to the STM32 (UART2 on GPIO44 TX / GPIO43 RX via the onboard
 * MAX13487EESA+). Owns an RX task that parses frames and hands them to
 * tb_ui_source, which is what ui/logic/ reads through ui_mock.h.
 *
 * Start this before ui_runtime_init() so no button frame is dropped.
 */
esp_err_t tb_link_start(void);

/* Send one frame to the STM32. Thread-safe. Returns ESP_ERR_TIMEOUT if the
 * TX FIFO stays full — the caller decides whether losing the frame matters. */
esp_err_t tb_link_send(tb_frame_kind_t kind, const void *payload, uint8_t len);

/* Convenience wrapper for TB_FRAME_CMD. */
esp_err_t tb_link_send_cmd(tb_cmd_t cmd);

/*
 * Hand the inference result back to the STM32, which owns the LoRa SX1278.
 * tag may be NULL (no RFID yet). confidence is 0..1 and is quantized to 0..100
 * on the wire.
 */
esp_err_t tb_link_send_result(ui_priority_t priority, float confidence, const char *tag);

/* Diagnostics for a status screen / log line. */
uint32_t tb_link_frames_ok(void);
uint32_t tb_link_crc_errors(void);

#endif /* TB_LINK_H */
