#include "tb_link_i2c.h"

#include <string.h>

#include "bsp/esp32_s3_touch_lcd_4.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/i2c_struct.h" /* link_bus_wedged(): the register IDF never clears */

#include "tb_frame.h" /* the priority conversion + its "no score" byte */
#include "tb_i2c_codec.h"
#include "tb_ui_source.h"
#include "bp_capture.h" /* the wave block feeds this board's BP inference */
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

/*
 * The peripheral register block for the BSP's I2C port. BSP_I2C_NUM is fixed at
 * build time (CONFIG_BSP_I2C_NUM), so this is a compile-time constant address,
 * not a runtime branch on every access.
 */
#define TB_I2C_HW (BSP_I2C_NUM == 0 ? &I2C0 : &I2C1)

static i2c_master_dev_handle_t s_dev;

/*
 * Detect and clear the ESP32-S3 I2C master's own permanent-wedge state.
 *
 * The root cause is in IDF v6.0.2, not this project: on any transaction error
 * the master driver calls i2c_ll_master_clr_bus(), which on the S3 sets
 * scl_sp_conf.scl_rst_slv_en = 1 to emit 9 recovery pulses -- but the "clear
 * done" poll (i2c_ll_master_is_bus_clear_done) hardcodes false on the S3, so the
 * driver's disarm write (clr_bus(..,false)) is never reached and the bit stays 1
 * forever. From then on every transaction re-arms it, SCL never actually moves,
 * and each attempt fails with a ~26 ms timeout. The bus is dead until reboot.
 *
 * link_bus_wedged() reads that stuck bit directly -- no transaction, so it is
 * safe to call every poll and cannot itself hang. link_bus_unwedge() resets the
 * peripheral FSM (which also runs the clear-bus that the driver leaves armed),
 * clears the bit by hand, and commits it via the shadow-register update bit.
 * Must be called holding bsp_i2c_lock(): it pokes the same peripheral a
 * concurrent transaction would be using.
 */
static bool link_bus_wedged(void)
{
    return TB_I2C_HW->scl_sp_conf.scl_rst_slv_en != 0;
}

static void link_bus_unwedge(void)
{
    (void)i2c_master_bus_reset(bsp_i2c_get_handle());
    TB_I2C_HW->scl_sp_conf.scl_rst_slv_en = 0;
    TB_I2C_HW->ctr.conf_upgate = 1; /* commit the shadowed scl_sp_conf write */
    ESP_LOGW(TAG, "I2C master un-wedged (cleared scl_rst_slv_en)");
}

static uint32_t s_polls_ok;
static uint32_t s_polls_failed;
static uint8_t s_btn_prev;   /* confirmed state, the diff baseline */
static uint8_t s_btn_last;   /* last raw reading, for 2-poll confirmation */
static bool s_btn_primed;
static uint8_t s_seq_prev;
static bool s_seq_seen;
static uint32_t s_seq_stalled;

static void on_button_edge(uint8_t index, bool pressed, void *ctx)
{
    (void)ctx;
    /*
     * Logged, not silent, because a phantom press is indistinguishable from a
     * real one once it has become a screen change. On 2026-09-04 an idle box
     * navigated to Scanning and raised the power-off dialog on its own -- both
     * are ButtonBar actions (cell1 and cell2 on Home), reachable from this path
     * OR from a spurious GT911 touch, and nothing in the log said which. With
     * this line the next occurrence answers it: edges here mean the STM32's
     * buttons byte, silence means the touch panel.
     */
    ESP_LOGI(TAG, "button %u %s", (unsigned)index, pressed ? "down" : "up");
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

    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return;
    }
    err = read_block(TB_REG_PROTO_VER, raw, sizeof(raw));
    bsp_i2c_unlock();

    if (err != ESP_OK) {
        ++s_polls_failed;
        /*
         * The button baseline dies with the link, and that is the fix for "the
         * idle box jumped to Scanning and put up the power-off dialog by
         * itself" (2026-09-04).
         *
         * Buttons are a STATE bitmask diffed against the previous poll. While
         * the link is down no polls land, so the baseline stays whatever it was
         * minutes ago -- and when the STM32 comes back (a reset, a reconnected
         * jumper, the 15-20 s slave stall recovering) its first byte can differ
         * in several bits at once. One diff then fires an edge for EVERY changed
         * bit, in ascending index order, and on Home that walks straight through
         * Scan (cell1) and Power (cell2): a screen change and a modal, from an
         * untouched box.
         *
         * Un-priming means the first poll after any gap SEEDS instead of
         * diffing, so the operator's own next press is the first edge. Nothing
         * real is lost: a press held across a link outage is not an event
         * anyone can attribute.
         */
        s_btn_primed = false;
        s_btn_last = 0U;
        /* Debug, not warning: with no STM32 attached this fires 20x/second and
         * would bury every other log line. The Home status dots are the
         * user-visible signal, and `stats` has the count. */
        ESP_LOGD(TAG, "poll failed: %s", esp_err_to_name(err));
        return;
    }

    if (!tb_i2c_decode_vitals(raw, &v)) {
        ++s_polls_failed;
        /* Same reason as the read failure above: a rejected block is a poll that
         * did not happen, so the button baseline must not survive it either. */
        s_btn_primed = false;
        s_btn_last = 0U;
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

    /*
     * State -> edges, with the state confirmed by TWO CONSECUTIVE POLLS
     * (100 ms apart).
     *
     * The wire carries a state bitmask, so a single corrupted or noisy poll used
     * to become a real button event. On 2026-09-04 an untouched box navigated to
     * Scanning and raised the power-off dialog on its own, twice, and Home's
     * cell1/cell2 are exactly Scan and Power -- ascending index order out of one
     * diff. Un-priming across link gaps (above) did not stop it, so the noise is
     * inside a run of GOOD polls: the STM32's PB12..PB15 read whatever the pins
     * do, and this build has no physical buttons wired to them.
     *
     * A press a human makes lasts far longer than 100 ms, so requiring the same
     * byte twice costs nothing real and rejects every single-poll glitch: a
     * glitch changes s_btn_last without ever confirming, and the next honest
     * poll changes it straight back. Seeding the baseline goes through the same
     * two-poll rule, which is what a reconnecting STM32 needs.
     */
    {
        const uint8_t now = tb_i2c_buttons(raw);

        if (now == s_btn_last) {
            if (!s_btn_primed) {
                s_btn_prev = now;
                s_btn_primed = true;
            } else if (now != s_btn_prev) {
                /* Same new state on two consecutive polls: a real change. */
                s_btn_prev = tb_i2c_diff_buttons(s_btn_prev, now,
                                                 on_button_edge, NULL);
            }
        }
        s_btn_last = now;
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

    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return;
    }
    if (write_reg(TB_REG_HOST_BATTERY, pct) != ESP_OK) {
        /* Debug for the same reason poll failures are: with no STM32 attached
         * this would fire every 5s forever. A stale battery is the mildest
         * symptom of a dead link, and the status dots already show that. */
        ESP_LOGD(TAG, "battery %u not delivered", (unsigned)pct);
    }
    bsp_i2c_unlock();
}

/*
 * Downlink RSSI, read separately from the vitals block. See TB_REG_LORA_RSSI for
 * why it sits at 0x30 rather than inside the block: the 50 ms poll stays exactly
 * 0x30 bytes, so an STM32 built before this register existed keeps answering it
 * unchanged and only this extra read comes back as its 0xFF pad.
 *
 * 1 Hz because the station polls each node once per LORA_POLL_PERIOD_MS (15 s),
 * so the value can only change every 15 s -- reading it at 20 Hz would be 19
 * transactions out of 20 spent re-reading a byte that cannot have moved, on a bus
 * shared with the touch controller. 1 Hz is still 15 reads per possible change,
 * which is enough to catch it promptly while walking the box around.
 */
#define TB_RSSI_EVERY (1000U / TB_POLL_MS)

static void poll_rssi_if_due(void)
{
    static uint32_t s_ticks;
    uint8_t raw;

    if ((s_ticks++ % TB_RSSI_EVERY) != 0U) {
        return; /* first pass runs, so the bar has a value within a poll of boot */
    }

    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return;
    }
    /*
     * A failed read leaves the last good value on screen rather than blanking
     * it, which is the opposite of what the battery does -- and deliberately so.
     * The battery is a safety number that must never look better than reality;
     * RSSI is a measurement someone is reading off the screen while walking, and
     * blanking it on one lost transaction would make the number unreadable
     * exactly when it is being used. A dead link blanks it anyway, via the
     * link_never_seen path in the status dots.
     */
    if (read_block(TB_REG_LORA_RSSI, &raw, 1U) == ESP_OK) {
        bsp_i2c_unlock();
        tb_ui_source_on_rssi((int8_t)raw);
        return;
    }
    bsp_i2c_unlock();
    ESP_LOGD(TAG, "rssi read failed");
}

/*
 * The waveform block (TB_REG_PPG_BASE, 124 bytes): the raw material for this
 * board's own BP inference, bp_capture.c.
 *
 * Every poll, not every Nth: the ring holds 20 samples = 200 ms, and the poll
 * is 50 ms, so 4 reads per turnover is already the slowest safe cadence. The
 * transaction is ~25 ms of the 50 kHz bus, which is why it only runs while a
 * capture is active -- bp_capture_capturing() -- plus one drain after the
 * freeze, so the tail of the window is not stranded in the ring.
 *
 * A GAP POISONS THE MODEL, not just the plot: the features are inter-beat
 * timing statistics, so a hole shifts every PAT/PTT after it. `dropped` from
 * tb_wave_take() nonzero, or a read failure, resets the capture; the model
 * then sees a short window and refuses, and the triage imputes 129.7. Restart
 * beats inventing.
 */
static void poll_wave_if_due(void)
{
    static tb_wave_block_t s_wave; /* 124 B in BSS: poll stack is 3072 bytes */
    static uint32_t s_last_total;
    static bool s_was_capturing;

    const bool capturing = bp_capture_capturing();
    if (!capturing && !s_was_capturing) {
        return;
    }
    /* One drain pass after the freeze: the last samples before measure-done
     * are still in the ring, and the accumulator deserves them. */
    const bool last_pass = !capturing && s_was_capturing;
    /* First pass of a new capture. s_last_total is whatever the PREVIOUS
     * measurement left, while the STM32's counter has been advancing at 100 Hz
     * the whole time in between -- so the delta is minutes of samples and
     * tb_wave_take() rightly calls it a gap. Seeding instead of consuming makes
     * the first pass mean "start counting here", which is what a measurement
     * beginning actually is; without it every measurement opened with a
     * spurious "wave gap (15319 lost)" and a restart. */
    const bool first_pass = capturing && !s_was_capturing;
    s_was_capturing = capturing;

    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return;
    }
    const esp_err_t err =
        read_block(TB_REG_PPG_BASE, (uint8_t *) &s_wave, sizeof(s_wave));
    bsp_i2c_unlock();

    if ((err != ESP_OK) || last_pass) {
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "wave read failed");
            if (capturing) {
                /* Same cure as a turnover: restart rather than leave a hole.
                 * The tail-end read failure after freeze needs no restart --
                 * the window is already closed. */
                ESP_LOGW(TAG, "wave read failed -- BP capture restarting");
                bp_capture_start();
                s_last_total = 0U;
            }
        }
        return;
    }

    tb_wave_sample_t out[TB_PPG_RING];
    uint32_t dropped = 0U;

    if (first_pass) {
        /* Keep the ring's 200 ms of pre-roll: it is the same finger a fifth of a
         * second earlier, and the biquads want samples to settle on. */
        s_last_total = s_wave.total - ((s_wave.total < TB_PPG_RING)
                                       ? s_wave.total : TB_PPG_RING);
    }

    const uint32_t n = tb_wave_take(&s_wave, &s_last_total, out, &dropped);

    if (dropped != 0U) {
        ESP_LOGW(TAG, "wave gap (%u lost) -- BP capture restarting",
                 (unsigned) dropped);
        bp_capture_start(); /* reset accumulator + filters, keep capturing */
        s_last_total = s_wave.total;
        return;
    }

    for (uint32_t i = 0U; i < n; ++i) {
        /* Take order is oldest first. ir/red are wire values (>>2) -- unpack
         * to counts; ecg is already raw. bp_capture_wave_push() drops the
         * samples after the last read because the freeze landed first. */
        bp_capture_wave_push(tb_ppg_unpack(out[i].ir),
                             tb_ppg_unpack(out[i].red), out[i].ecg);
    }
}

static void poll_task(void *arg)
{
    (void)arg;
    for (;;) {
        /*
         * Before anything else, because everything else is a transaction and a
         * wedged master fails all of them at ~26 ms a go. This task polls at
         * 20 Hz, so it is the natural place to notice: one poll after the fault
         * the bus is back, without the reboot that used to be the only cure.
         */
        if (link_bus_wedged() && bsp_i2c_lock(TB_I2C_TIMEOUT)) {
            link_bus_unwedge();
            bsp_i2c_unlock();
        }
        poll_once();
        poll_rssi_if_due();
        poll_wave_if_due();
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
        /*
         * 50 kHz to the STM32, while the GT911 and the expander keep their
         * 400 kHz -- the driver reprograms the bus timing per transaction from
         * the device handle, so the two coexist on one bus.
         *
         * Not a protocol requirement, an ANALOG one. This bus was laid out for
         * three chips on one PCB, and the pull-ups on the display board are all
         * there is: a WeAct Black Pill has no I2C pull-ups of its own on PB3 or
         * PB10. Adding jumper wires and a second board adds bus capacitance that
         * those pull-ups were never sized for, and the rise time goes out of the
         * 1000 ns the standard allows. A slow edge is read as the wrong level,
         * which is where the misplaced STARTs, the BERRs and the latched BUSY
         * flag come from -- F411 erratum 2.8.7 is literally about the analog
         * filter mis-sampling an edge.
         *
         * Halving the clock does not fix the rise time; it gives the line twice
         * as long to finish rising before the bit is sampled. That is enough to
         * turn a marginal link into a working one, and it costs 4.9 ms per poll
         * instead of 2.5 -- still 10% of the 50 ms period.
         *
         * THIS IS A CRUTCH, NOT THE FIX. The fix is 2.2k-4.7k pull-ups on SDA and
         * SCL and a short dedicated ground between the two boards. Put those in
         * and this can go back to 100000.
         */
        .scl_speed_hz = 50000,
        /*
         * 20 ms of clock-stretch tolerance, not the 2 ms default.
         *
         * The default is 2000 us, and on this SoC that is 2000 us of XTAL at
         * 40 MHz rounded up to the next power of two -- 3.28 ms, measured from
         * the register the driver writes. The STM32 stretches SCL from its I2C2
         * ISR, which sits at the same NVIC preempt priority as its 497.5 Hz ADC
         * ISR, its DMA and its EXTI, so it cannot preempt any of them; a stretch
         * of several ms is normal there and not a fault. Exceeding this budget is
         * what logs "I2C hardware timeout detected", and each expiry costs an FSM
         * reset plus a fire-and-forget 9-pulse bus clear -- on a bus the GT911
         * touch controller shares and waits on with portMAX_DELAY.
         *
         * 20000 is the figure the one worked example of this exact pairing
         * (ESP32-S3 master, STM32 slave) settled on upstream, and the IDF I2C
         * guide warns a slave "can even stretch for 12 ms". A longer ceiling
         * costs nothing when nothing is stretching: it is a deadline, not a delay.
         */
        .scl_wait_us = 20000,
    };
    esp_err_t err;

    if (bus == NULL) {
        /* The BSP brings the bus up during display init; being called first is
         * a wiring-order bug in app_main, not a runtime condition. */
        ESP_LOGE(TAG, "I2C bus not up yet -- call after bsp_display_start()");
        return ESP_ERR_INVALID_STATE;
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
    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return ESP_ERR_TIMEOUT;
    }
    err = write_reg(TB_REG_CMD, cmd);
    bsp_i2c_unlock();

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cmd 0x%02x not delivered: %s", (unsigned)cmd,
                 esp_err_to_name(err));
    }
    return err;
}

/*
 * Both halves of a result write, in the one order the STM32 accepts.
 *
 * Priority first, confidence second: the slave latches the pair as complete when
 * TB_REG_CONFIDENCE is written, so this order is load-bearing. Two separate
 * transactions rather than one 2-byte write because the slave's pointer
 * auto-increment is exercised either way and separate writes keep the
 * "confidence last" guarantee explicit.
 *
 * Takes the wire byte, not a ui_priority_t, because there are five wire values
 * and only four colours -- see tb_link_send_unscored().
 */
static esp_err_t send_result_bytes(uint8_t wire_priority, uint8_t pct)
{
    esp_err_t err;

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return ESP_ERR_TIMEOUT;
    }
    err = write_reg(TB_REG_PRIORITY, wire_priority);
    if (err == ESP_OK) {
        err = write_reg(TB_REG_CONFIDENCE, pct);
    }
    bsp_i2c_unlock();

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RESULT not delivered: %s -- station will miss this "
                      "triage", esp_err_to_name(err));
    }
    return err;
}

esp_err_t tb_link_send_result(ui_priority_t priority, float confidence,
                              const char *tag)
{
    (void)tag; /* see the header: the STM32 already has the tag */

    if (confidence < 0.0f) {
        confidence = 0.0f;
    } else if (confidence > 1.0f) {
        confidence = 1.0f;
    }
    /*
     * tb_frame_priority_to_wire() converts ui_priority_t order (RED, YELLOW,
     * GREEN, BLACK) to the LoRa numeric alias (0=BLACK 1=RED 2=YELLOW
     * 3=GREEN). Skipping it silently swaps RED and BLACK.
     */
    return send_result_bytes(tb_frame_priority_to_wire((int)priority),
                             (uint8_t)((confidence * 100.0f) + 0.5f));
}

esp_err_t tb_link_send_unscored(void)
{
    /* Confidence 0 with it: a confidence for a verdict that does not exist
     * describes nothing, and the station drops the pair together anyway. */
    return send_result_bytes(TB_PRIORITY_WIRE_NONE, 0U);
}

/*
 * The BP result travels to the STM32 so its LoRa packet can carry it -- the
 * same "measured here, transmitted there" handoff as TB_REG_HOST_BATTERY.
 * SYS first, DIA last: the slave latches the pair on DIA's final byte
 * (tb_regs.h), so this order is load-bearing exactly like
 * tb_link_send_result()'s PRIORITY-then-CONFIDENCE.
 *
 * Each value is two write_reg() calls (register pointer + one byte), so the
 * u16s go out little-endian by construction: low byte at 0x44, high at 0x45.
 */
esp_err_t tb_link_send_bp(uint16_t sys, uint16_t dia)
{
    esp_err_t err;

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return ESP_ERR_TIMEOUT;
    }
    err = write_reg(TB_REG_HOST_BP_SYS, (uint8_t) (sys & 0xFFU));
    if (err == ESP_OK) {
        err = write_reg(TB_REG_HOST_BP_SYS + 1U, (uint8_t) (sys >> 8));
    }
    if (err == ESP_OK) {
        err = write_reg(TB_REG_HOST_BP_DIA, (uint8_t) (dia & 0xFFU));
    }
    if (err == ESP_OK) {
        err = write_reg(TB_REG_HOST_BP_DIA + 1U, (uint8_t) (dia >> 8));
    }
    bsp_i2c_unlock();

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BP not delivered: %s -- the packet will omit it",
                 esp_err_to_name(err));
    }
    return err;
}

/*
 * The four things the operator or the model knows and the STM32 cannot measure:
 * the typed respiratory rate, the age and sex the model scored, and the raw ESI
 * behind the colour. Same backwards direction as TB_REG_HOST_BATTERY and the BP
 * pair, and the same shape: one lock for the group, one write_reg() per byte.
 *
 * The argument order mirrors the register order (0x48..0x4B) so a swapped pair is
 * visible in this call rather than on the dashboard.
 *
 * CALL IT BEFORE tb_link_send_result()/tb_link_send_unscored(). The slave latches
 * the verdict as complete when TB_REG_CONFIDENCE is written -- see
 * send_result_bytes() -- and the ESI is part of that verdict, so an ESI written
 * afterwards belongs to the NEXT patient while this one has already been
 * published under the previous value. RR/age/sex are patient facts rather than
 * parts of the verdict and only have to precede it; sending all four in one group
 * is the version of that rule with nothing left to remember.
 */
esp_err_t tb_link_send_patient(uint8_t rr_brpm, uint8_t age_years,
                               uint8_t gender_ascii, uint8_t esi)
{
    esp_err_t err;

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return ESP_ERR_TIMEOUT;
    }
    err = write_reg(TB_REG_HOST_RR, rr_brpm);
    if (err == ESP_OK) {
        err = write_reg(TB_REG_HOST_AGE, age_years);
    }
    if (err == ESP_OK) {
        err = write_reg(TB_REG_HOST_GENDER, gender_ascii);
    }
    if (err == ESP_OK) {
        err = write_reg(TB_REG_HOST_ESI, esi);
    }
    bsp_i2c_unlock();

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "patient fields not delivered: %s -- the packet will omit "
                      "rr/age/sex and the ESI behind the colour",
                 esp_err_to_name(err));
    }
    return err;
}

/*
 * The STM32's 96-bit factory UID, in its own 12-byte read outside the vitals
 * block (see TB_REG_UID). On demand only: it is constant for the life of the
 * chip, so there is nothing to poll and no reason to spend bus time on it.
 *
 * No log line here, unlike the writes above: the only caller is the `uid`
 * console command, and an operator watching for the answer is better served by
 * the error printed beside it than by a warning in the same stream.
 *
 * What the bytes MEAN is the caller's problem. An STM32 built before this block
 * existed answers with twelve 0xFF pad bytes, which is not a UID the F411 can
 * report -- tb_debug.c's `uid` says so rather than printing them.
 */
esp_err_t tb_link_read_uid(uint8_t *uid)
{
    esp_err_t err;

    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!bsp_i2c_lock(TB_I2C_TIMEOUT)) {
        return ESP_ERR_TIMEOUT;
    }
    err = read_block(TB_REG_UID, uid, TB_REG_UID_LEN);
    bsp_i2c_unlock();
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
