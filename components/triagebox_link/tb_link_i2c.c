#include "tb_link_i2c.h"

#include <string.h>

#include "bsp/esp32_s3_touch_lcd_4.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tb_frame.h" /* tb_frame_priority_to_wire() only -- see below */
#include "tb_i2c_codec.h"
#include "tb_ui_source.h"
#include "ui_board.h" /* ui_board_battery(): the gauge lives on our side */

static const char *TAG = "tb_link_i2c";

/*
 * 50 ms matches the LVGL runtime timer, so the UI never polls stale data twice
 * and never waits on a poll that has not happened. Button latency is one poll
 * plus the STM32's ~30 ms debounce -- about 80 ms worst case, below the ~100 ms
 * where a press starts to feel laggy.
 */
#define TB_POLL_MS      50
#define TB_TASK_STACK   3072
/* Below the LVGL task (which esp_lvgl_port runs at 4 by default) so a poll
 * never delays a redraw; above idle so it is not starved. */
#define TB_TASK_PRIO    3
/* Generous: a slave that is clock-stretching behind its ADC ISR is normal, and
 * the alternative to waiting is a spurious "link down". */
#define TB_I2C_TIMEOUT  100

static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_lock; /* serialises our own reads vs writes */

static uint32_t s_polls_ok;
static uint32_t s_polls_failed;
static uint8_t s_btn_prev;
static bool s_btn_primed;
static uint8_t s_seq_prev;
static bool s_seq_seen;
static uint32_t s_seq_stalled;

static void on_button_edge(uint8_t index, bool pressed, void *ctx)
{
    (void)ctx;
    tb_ui_source_on_button(index, pressed);
}

/* Write the register pointer, then read the block. i2c_master_transmit_receive
 * issues both phases as one transaction with a repeated start, which is what
 * the STM32's AddrCallback expects. */
static esp_err_t read_block(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, TB_I2C_TIMEOUT);
}

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    const uint8_t out[2] = {reg, val};
    return i2c_master_transmit(s_dev, out, sizeof(out), TB_I2C_TIMEOUT);
}

static void poll_once(void)
{
    uint8_t raw[TB_REG_READ_END];
    vitals_t v;
    esp_err_t err;
    uint8_t seq;

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(TB_I2C_TIMEOUT)) != pdTRUE) {
        return;
    }
    err = read_block(TB_REG_PROTO_VER, raw, sizeof(raw));
    xSemaphoreGive(s_lock);

    if (err != ESP_OK) {
        ++s_polls_failed;
        /* Debug, not warning: with no STM32 attached this fires 20x/second and
         * would bury every other log line. The Home status dots are the
         * user-visible signal, and `stats` has the count. */
        ESP_LOGD(TAG, "poll failed: %s", esp_err_to_name(err));
        return;
    }

    if (!tb_i2c_decode_vitals(raw, &v)) {
        ++s_polls_failed;
        /* Wrong protocol version: the two tb_regs.h copies have drifted. Warn
         * once per second rather than per poll -- it is a build-time mistake,
         * not a runtime condition, and it will not fix itself. */
        static uint32_t last_warn;
        if ((s_polls_failed - last_warn) >= (1000U / TB_POLL_MS)) {
            last_warn = s_polls_failed;
            ESP_LOGW(TAG, "proto_ver 0x%02x != 0x%02x -- tb_regs.h mismatch "
                          "between this build and the STM32",
                     (unsigned)raw[TB_REG_PROTO_VER], (unsigned)TB_PROTO_VER);
        }
        return;
    }

    ++s_polls_ok;
    tb_ui_source_mark_frame();
    tb_ui_source_on_vital(&v);
    tb_ui_source_on_status(raw[TB_REG_SENSOR_OK], raw[TB_REG_BATTERY],
                           (raw[TB_REG_SENSOR_OK] & TB_SENSOR_LORA) ? 1 : 0);

    /*
     * A frozen sequence counter means the STM32 answered (so the bus and the
     * ISR are fine) but its superloop is not running -- a hang in the DSP, or a
     * debugger halt. Distinguishable from "sensors quiet" only because seq
     * advances on every publish regardless of whether readings changed.
     */
    seq = tb_i2c_seq(raw);
    if (s_seq_seen && (seq == s_seq_prev)) {
        if (++s_seq_stalled == (2000U / TB_POLL_MS)) {
            ESP_LOGW(TAG, "STM32 answering but seq frozen at %u -- superloop "
                          "stalled?", (unsigned)seq);
        }
    } else {
        s_seq_stalled = 0;
    }
    s_seq_prev = seq;
    s_seq_seen = true;

    /* State -> edges. First successful poll seeds the baseline instead of
     * emitting presses for whatever happened to be held at boot. */
    {
        uint8_t now = tb_i2c_buttons(raw);
        if (!s_btn_primed) {
            s_btn_prev = now;
            s_btn_primed = true;
        } else {
            s_btn_prev = tb_i2c_diff_buttons(s_btn_prev, now, on_button_edge,
                                             NULL);
        }
    }

    /* RFID: the block already carries the tag, so no second transaction. */
    if (raw[TB_REG_RFID_LEN] > 0U) {
        rfid_t r = {0};
        uint8_t n = raw[TB_REG_RFID_LEN];

        if (n > RFID_TAG_CAPACITY - 1U) {
            n = RFID_TAG_CAPACITY - 1U;
        }
        memcpy(r.tag, &raw[TB_REG_RFID], n); /* not NUL-terminated on the wire */
        r.present = true;
        tb_ui_source_on_rfid(&r);
    } else {
        /*
         * "No tag" is information, not silence: it is the only evidence that the
         * STM32 has processed a START_SCAN and dropped the previous patient's
         * card. Without this push the gate in tb_ui_source_on_rfid() could never
         * open. See ui_mock_start_scan() for the race it closes.
         */
        const rfid_t empty = {0};

        tb_ui_source_on_rfid(&empty);
    }
}

/*
 * The battery percentage the STM32 cannot read for itself.
 *
 * The SW6106 fuel gauge sits at 0x3c on this same bus, but as a slave, so only
 * this board can read it -- and the LoRa packet is built on the STM32. Without
 * these few lines the node shows a real percentage on its own LCD while the
 * dashboard's battery column stays permanently blank, which is the state this
 * fixed. See TB_REG_HOST_BATTERY.
 *
 * Not every poll: a SoC that moves ~1% per several minutes does not need a write
 * every 50ms, and each attempt costs two addressed PMIC reads plus a write on a
 * bus shared with the touch controller. 5s is still 3 updates per LoRa cycle.
 */
#define TB_BATTERY_EVERY (5000U / TB_POLL_MS)

static void push_battery_if_due(void)
{
    static uint32_t s_ticks;
    uint8_t pct;

    if ((s_ticks++ % TB_BATTERY_EVERY) != 0U) {
        return; /* first pass runs, so the value is up within one poll of boot */
    }

    /*
     * 0xFF on a failed read, deliberately, rather than holding the last good
     * value. Same rule as the status bar: a frozen 80% while the pack drains is
     * worse than one blank cycle. Never 0 -- 0 is a flat pack, and the station
     * publishes it instead of omitting the key.
     *
     * The charging flag is dropped (NULL): the LoRa vital has no field for it,
     * so carrying it would need a wire change on a link the STM32 owns, for
     * something only useful next to the socket -- where the LCD already shows it.
     */
    if (!ui_board_battery(&pct, NULL)) {
        pct = 0xFFU;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(TB_I2C_TIMEOUT)) != pdTRUE) {
        return;
    }
    if (write_reg(TB_REG_HOST_BATTERY, pct) != ESP_OK) {
        /* Debug for the same reason poll failures are: with no STM32 attached
         * this would fire every 5s forever. A stale battery is the mildest
         * symptom of a dead link, and the status dots already show that. */
        ESP_LOGD(TAG, "battery %u not delivered", (unsigned)pct);
    }
    xSemaphoreGive(s_lock);
}

static void poll_task(void *arg)
{
    (void)arg;
    for (;;) {
        poll_once();
        push_battery_if_due();
        vTaskDelay(pdMS_TO_TICKS(TB_POLL_MS));
    }
}

esp_err_t tb_link_start(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TB_I2C_SLAVE_ADDR,
        /* Matches the STM32's I2C2 (Standard mode) and the rest of this bus.
         * A slave follows the master's clock, so this is the only place the
         * speed is decided. */
        .scl_speed_hz = 100000,
    };
    esp_err_t err;

    if (bus == NULL) {
        /* The BSP brings the bus up during display init; being called first is
         * a wiring-order bug in app_main, not a runtime condition. */
        ESP_LOGE(TAG, "I2C bus not up yet -- call after bsp_display_start()");
        return ESP_ERR_INVALID_STATE;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_device 0x%02x: %s", TB_I2C_SLAVE_ADDR,
                 esp_err_to_name(err));
        return err;
    }

    /* Probe for a clearer log line than 20 failed polls a second. Not fatal:
     * the STM32 may simply boot later, and the UI must run regardless. */
    if (i2c_master_probe(bus, TB_I2C_SLAVE_ADDR, 100) == ESP_OK) {
        ESP_LOGI(TAG, "STM32 found at 0x%02x", TB_I2C_SLAVE_ADDR);
    } else {
        ESP_LOGW(TAG, "no answer from 0x%02x -- check SDA/SCL and that the "
                      "STM32 is running", TB_I2C_SLAVE_ADDR);
    }

    if (xTaskCreate(poll_task, "tb_i2c", TB_TASK_STACK, NULL, TB_TASK_PRIO,
                    NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "i2c link up: addr=0x%02x poll=%dms", TB_I2C_SLAVE_ADDR,
             TB_POLL_MS);
    return ESP_OK;
}

esp_err_t tb_link_send_cmd(uint8_t cmd)
{
    esp_err_t err;

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(TB_I2C_TIMEOUT)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    err = write_reg(TB_REG_CMD, cmd);
    xSemaphoreGive(s_lock);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cmd 0x%02x not delivered: %s", (unsigned)cmd,
                 esp_err_to_name(err));
    }
    return err;
}

esp_err_t tb_link_send_result(ui_priority_t priority, float confidence,
                              const char *tag)
{
    uint8_t pct;
    esp_err_t err;

    (void)tag; /* see the header: the STM32 already has the tag */

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (confidence < 0.0f) {
        confidence = 0.0f;
    } else if (confidence > 1.0f) {
        confidence = 1.0f;
    }
    pct = (uint8_t)((confidence * 100.0f) + 0.5f);

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(TB_I2C_TIMEOUT)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    /*
     * Priority first, confidence second: the STM32 latches the pair as complete
     * when TB_REG_CONFIDENCE is written, so this order is load-bearing. Two
     * separate transactions rather than one 2-byte write because the slave's
     * pointer auto-increment is exercised either way and separate writes keep
     * the "confidence last" guarantee explicit.
     *
     * tb_frame_priority_to_wire() converts ui_priority_t order (RED, YELLOW,
     * GREEN, BLACK) to the LoRa numeric alias (0=BLACK 1=RED 2=YELLOW
     * 3=GREEN). Skipping it silently swaps RED and BLACK.
     */
    err = write_reg(TB_REG_PRIORITY, tb_frame_priority_to_wire((int)priority));
    if (err == ESP_OK) {
        err = write_reg(TB_REG_CONFIDENCE, pct);
    }
    xSemaphoreGive(s_lock);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RESULT not delivered: %s -- station will miss this "
                      "triage", esp_err_to_name(err));
    }
    return err;
}

uint32_t tb_link_frames_ok(void)
{
    return s_polls_ok;
}

uint32_t tb_link_crc_errors(void)
{
    /* Name kept from the RS485 transport so tb_debug's `stats` and the status
     * dots need no change. There is no CRC on I2C -- this counts failed
     * transactions and version rejects, which is the same signal: the link is
     * present but not delivering usable data. */
    return s_polls_failed;
}
