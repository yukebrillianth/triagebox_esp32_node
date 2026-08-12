#include "tb_link.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tb_ui_source.h"

/*
 * GPIO43/44 are the SP3485 RS485 pair on this board (AGENTS.md "Hardware
 * traps" #5). They are also the default UART0 console pins on ESP32-S3, which
 * is why this uses UART2: the console stays on USB Serial/JTAG
 * (CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y) and idf.py monitor keeps
 * working while the link is live.
 */
#define TB_UART_PORT  UART_NUM_2
/* Plain ints: uart_set_pin() takes int, so GPIO_NUM_* would only add a
 * dependency on the gpio driver component for no type safety. */
#define TB_UART_TX    44
#define TB_UART_RX    43
#define TB_UART_BAUD  115200

#define TB_RX_BUF_SZ  1024
#define TB_TX_BUF_SZ  512
#define TB_TASK_STACK 4096
#define TB_TASK_PRIO  10

static const char *TAG = "tb_link";

static tb_frame_parser_t s_parser;
static SemaphoreHandle_t s_tx_lock;

static void dispatch(const tb_frame_t *f)
{
    switch (f->kind) {
    case TB_FRAME_VITAL:
        if (f->len >= 12) {
            vitals_t v = {
                /* Little-endian u16 pairs; see tb_frame.h for the layout. */
                .hr     = (uint16_t)(f->payload[0] | (f->payload[1] << 8)),
                .spo2   = (uint16_t)(f->payload[2] | (f->payload[3] << 8)),
                .rr     = (uint16_t)(f->payload[4] | (f->payload[5] << 8)),
                .bp_sys = (uint16_t)(f->payload[6] | (f->payload[7] << 8)),
                .bp_dia = (uint16_t)(f->payload[8] | (f->payload[9] << 8)),
                .battery = f->payload[10],
                .valid = (f->payload[11] & TB_VITAL_FLAG_VALID) != 0,
            };
            tb_ui_source_on_vital(&v);
        }
        break;

    case TB_FRAME_BUTTON:
        if (f->len >= 2) {
            tb_ui_source_on_button(f->payload[0], f->payload[1] != 0);
        }
        break;

    case TB_FRAME_RFID: {
        rfid_t r = {0};
        uint8_t n = f->len;

        if (n > RFID_TAG_CAPACITY - 1U) {
            n = RFID_TAG_CAPACITY - 1U; /* payload is not NUL-terminated */
        }
        memcpy(r.tag, f->payload, n);
        r.present = n > 0;
        tb_ui_source_on_rfid(&r);
        break;
    }

    case TB_FRAME_STATUS:
        if (f->len >= 2) {
            tb_ui_source_on_status(f->payload[0], f->payload[1]);
        }
        break;

    default:
        /* CMD/RESULT are ESP32->STM32; seeing them means the line is looped
         * back (see the loopback test in docs/firmware-architecture.md). */
        ESP_LOGD(TAG, "ignoring frame kind 0x%02x", (unsigned)f->kind);
        break;
    }
}

static void rx_task(void *arg)
{
    uint8_t buf[128];

    (void)arg;
    for (;;) {
        int n = uart_read_bytes(TB_UART_PORT, buf, sizeof(buf), pdMS_TO_TICKS(50));
        for (int i = 0; i < n; i++) {
            tb_frame_t f;
            if (tb_frame_feed(&s_parser, buf[i], &f)) {
                dispatch(&f);
            }
        }
    }
}

esp_err_t tb_link_start(void)
{
    const uart_config_t cfg = {
        .baud_rate = TB_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err;

    s_tx_lock = xSemaphoreCreateMutex();
    if (s_tx_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = uart_driver_install(TB_UART_PORT, TB_RX_BUF_SZ, TB_TX_BUF_SZ, 0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_param_config(TB_UART_PORT, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(TB_UART_PORT, TB_UART_TX, TB_UART_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }

    /* TODO(hardware): README.md says the onboard SP3485 switches TX/RX
     * automatically, so plain UART mode is correct and no DE/RE pin is needed.
     * Verify against the schematic of the physical board revision. If DE turns
     * out to be manual, switch to uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)
     * and give RTS a pin — note the GPIO budget in AGENTS.md has no obvious
     * free candidate, so that would need a peripheral to be dropped. */

    memset(&s_parser, 0, sizeof(s_parser));

    if (xTaskCreate(rx_task, "tb_rx", TB_TASK_STACK, NULL, TB_TASK_PRIO, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "uart%d up: tx=%d rx=%d @%d", TB_UART_PORT, TB_UART_TX, TB_UART_RX,
             TB_UART_BAUD);
    return ESP_OK;
}

esp_err_t tb_link_send(tb_frame_kind_t kind, const void *payload, uint8_t len)
{
    uint8_t wire[TB_FRAME_MAX];
    size_t n = tb_frame_encode(kind, payload, len, wire, sizeof(wire));
    int written;

    if (n == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_tx_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_tx_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    written = uart_write_bytes(TB_UART_PORT, wire, n);
    xSemaphoreGive(s_tx_lock);

    return (written == (int)n) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t tb_link_send_cmd(tb_cmd_t cmd)
{
    uint8_t payload = (uint8_t)cmd;
    return tb_link_send(TB_FRAME_CMD, &payload, 1);
}

esp_err_t tb_link_send_result(ui_priority_t priority, float confidence, const char *tag)
{
    uint8_t payload[2 + RFID_TAG_CAPACITY];
    uint8_t len = 2;

    if (confidence < 0.0f) {
        confidence = 0.0f;
    } else if (confidence > 1.0f) {
        confidence = 1.0f;
    }

    payload[0] = tb_frame_priority_to_wire((int)priority);
    payload[1] = (uint8_t)(confidence * 100.0f + 0.5f);

    if (tag != NULL) {
        size_t tag_len = strnlen(tag, RFID_TAG_CAPACITY - 1U);
        if (tag_len > TB_FRAME_PAYLOAD_MAX - 2U) {
            tag_len = TB_FRAME_PAYLOAD_MAX - 2U;
        }
        memcpy(&payload[2], tag, tag_len);
        len = (uint8_t)(2U + tag_len);
    }

    return tb_link_send(TB_FRAME_RESULT, payload, len);
}

uint32_t tb_link_frames_ok(void)
{
    return s_parser.frames_ok;
}

uint32_t tb_link_crc_errors(void)
{
    return s_parser.crc_errors;
}
