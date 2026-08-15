/*
 * Serial debug console: inject frames that would normally come from the STM32,
 * so the whole triage flow can be demoed before the sensor board exists.
 *
 * Build-gated by TB_DEBUG_CONSOLE (see components/triagebox_debug/CMakeLists.txt)
 * -- do not ship it enabled: it lets anyone fake patient vitals over USB.
 *
 * Type `help` in `idf.py monitor` for the command list.
 */
#include "tb_debug.h"

#include <stdlib.h>
#include <string.h>

#include "bsp/esp32_s3_touch_lcd_4.h"
#include "driver/i2c_master.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tb_i2c_codec.h"
#include "tb_link_i2c.h"
#include "tb_svm.h"
#include "tb_ui_source.h"
#include "ui_mock.h"
#include "ui_status.h"

static const char *TAG = "tb_debug";

static int cmd_rfid(int argc, char **argv)
{
    rfid_t r = {0};
    const char *tag = (argc > 1) ? argv[1] : "3021";

    strncpy(r.tag, tag, RFID_TAG_CAPACITY - 1U);
    r.present = true;
    tb_ui_source_mark_frame();
    tb_ui_source_on_rfid(&r);
    printf("injected RFID '%s'\n", r.tag);
    return 0;
}

static int cmd_vital(int argc, char **argv)
{
    /* Defaults are a healthy adult; override any prefix of the arguments. */
    vitals_t v = {
        .hr = 90, .spo2 = 98, .rr = 18, .bp_sys = 120, .bp_dia = 80,
        .battery = 80, .valid = true,
    };

    if (argc > 1) v.hr     = (uint16_t)atoi(argv[1]);
    if (argc > 2) v.spo2   = (uint16_t)atoi(argv[2]);
    if (argc > 3) v.rr     = (uint16_t)atoi(argv[3]);
    if (argc > 4) v.bp_sys = (uint16_t)atoi(argv[4]);
    if (argc > 5) v.bp_dia = (uint16_t)atoi(argv[5]);

    tb_ui_source_mark_frame();
    tb_ui_source_on_vital(&v);
    printf("injected VITAL hr=%u spo2=%u rr=%u bp=%u/%u\n",
           v.hr, v.spo2, v.rr, v.bp_sys, v.bp_dia);
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    /* No args = everything healthy, which is what a demo wants. */
    uint8_t mask = (argc > 1) ? (uint8_t)strtoul(argv[1], NULL, 0) : UI_SENSOR_ALL;
    int lora = (argc > 2) ? atoi(argv[2]) : 1;

    tb_ui_source_mark_frame();
    tb_ui_source_on_status(mask, 80, lora);
    printf("injected STATUS sensors=0x%02x lora=%d\n", mask, lora);
    return 0;
}

static int cmd_btn(int argc, char **argv)
{
    uint8_t idx = (argc > 1) ? (uint8_t)atoi(argv[1]) : 0;

    if (idx > 3U) {
        printf("button index must be 0..3\n");
        return 1;
    }
    /* Press and release: the keypad indev needs both edges. */
    tb_ui_source_on_button(idx, true);
    vTaskDelay(pdMS_TO_TICKS(120));
    tb_ui_source_on_button(idx, false);
    printf("pressed button %u\n", idx);
    return 0;
}

/*
 * Sweep the shared I2C bus (SDA GPIO15 / SCL GPIO7) and name what answers.
 * The bus is the BSP's -- bsp_i2c_get_handle(), never a second bus -- so this
 * only works after bsp_display_start(), which is why the REPL starts last.
 *
 * Addresses 0x00-0x07 and 0x78-0x7f are reserved by the I2C spec and are not
 * probed: a general-call write can reconfigure every device on the bus at once.
 */
static const char *i2c_known(uint8_t addr)
{
    switch (addr) {
    case 0x14: return "GT911 touch (fallback addr)";
    case 0x1e: return "SW6106 PMIC (if 0x3C in the datasheet is the write byte)";
    case 0x20: return "TCA9554 expander (A2:A0=000)";
    case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27: return "TCA9554 expander (strapped)";
    case 0x3c: return "SW6106 PMIC (datasheet 9.17: slave address 0x3C)";
    case 0x42: return "STM32F411 link slave (tb_regs.h) -- try `i2clink`";
    case 0x51: return "PCF85063A RTC";
    case 0x5d: return "GT911 touch";
    case 0x6a: case 0x6b: return "QMI8658 IMU -- marked NC on V3.0, so unexpected";
    default:   return "unknown -- something on the Interface header?";
    }
}

static int cmd_i2c(int argc, char **argv)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    /* 50 ms: a device that clock-stretches (the STM32 will) needs more than the
     * 10 ms every example uses, and a scan is not latency-sensitive. */
    const int timeout_ms = (argc > 1) ? atoi(argv[1]) : 50;
    /* Probe once and remember, rather than sweeping twice for the grid and the
     * list -- at 50 ms a miss, a second pass costs seconds. */
    bool present[0x78] = {false};
    int found = 0;

    if (bus == NULL) {
        printf("I2C bus not up -- bsp_i2c_init() has not run\n");
        return 1;
    }

    printf("scanning 0x08..0x77 on SDA=%d SCL=%d (timeout %d ms)\n",
           BSP_I2C_SDA, BSP_I2C_SCL, timeout_ms);
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

    for (uint8_t hi = 0x00; hi <= 0x70; hi = (uint8_t)(hi + 0x10)) {
        printf("%02x: ", hi);
        for (uint8_t lo = 0x00; lo <= 0x0f; lo++) {
            uint8_t addr = (uint8_t)(hi | lo);

            if (addr < 0x08U || addr > 0x77U) {
                printf("   ");
                continue;
            }
            present[addr] = (i2c_master_probe(bus, addr, timeout_ms) == ESP_OK);
            if (present[addr]) {
                printf("%02x ", addr);
                found++;
            } else {
                printf("-- ");
            }
        }
        printf("\n");
    }

    printf("\n%d device(s):\n", found);
    for (uint8_t addr = 0x08U; addr < 0x78U; addr++) {
        if (present[addr]) {
            printf("  0x%02x  %s\n", addr, i2c_known(addr));
        }
    }
    if (found == 0) {
        printf("  nothing answered -- check pull-ups, or SDA stuck low after a\n"
               "  soft reset (AGENTS.md item 20 wants bus recovery for exactly this)\n");
    }
    printf("\n");
    return 0;
}

/*
 * Read registers from one device, to identify whatever `i2c` turned up.
 *
 * Read-only on purpose: there is no `i2cwrite`. A stray write to the SW6106 at
 * 0x3c could latch the power path off mid-triage, and the datasheet publishes no
 * register map to write against.
 *
 * Two transaction shapes, because devices disagree about which they accept:
 *   default -- write pointer, repeated START, read   (the usual convention)
 *   "split" -- write pointer, STOP, separate read    (SW6106 datasheet 9.17 is
 *              drawn as "1st/2nd/3rd step", which hints at this)
 * Note many expanders (TCA9554 included) do not auto-increment the pointer, so
 * count > 1 re-reads the same register rather than walking upward.
 */
static int cmd_i2creg(int argc, char **argv)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    i2c_master_dev_handle_t dev = NULL;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,   /* slowest thing on the bus tolerates it */
    };
    uint8_t reg;
    uint8_t buf[16] = {0};
    int count;
    bool split;
    esp_err_t err;

    if (argc < 3) {
        printf("usage: i2creg <addr> <reg> [count 1..%d] [split]  (values accept 0x)\n",
               (int)sizeof(buf));
        printf("  TCA9554: 0=input 1=output 2=polarity 3=config (0xff = all inputs,\n");
        printf("           i.e. nobody configured it). Pointer does NOT auto-increment,\n");
        printf("           so read one register at a time.\n");
        printf("  SW6106 : 0xb0 is the only register the datasheet names; try `split`\n");
        printf("           if a plain read returns 00.\n");
        return 1;
    }
    if (bus == NULL) {
        printf("I2C bus not up -- bsp_i2c_init() has not run\n");
        return 1;
    }

    cfg.device_address = (uint16_t)strtoul(argv[1], NULL, 0);
    reg   = (uint8_t)strtoul(argv[2], NULL, 0);
    /* "split" is accepted in either trailing position, so `i2creg a r split`
     * works without having to pass a count you did not care about. */
    split = false;
    count = 1;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "split") == 0) {
            split = true;
        } else {
            count = atoi(argv[i]);
        }
    }

    if (cfg.device_address < 0x08U || cfg.device_address > 0x77U) {
        printf("address must be 0x08..0x77\n");
        return 1;
    }
    if (count < 1 || count > (int)sizeof(buf)) {
        printf("count must be 1..%d\n", (int)sizeof(buf));
        return 1;
    }

    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) {
        printf("could not add device 0x%02x\n", (unsigned)cfg.device_address);
        return 1;
    }
    if (split) {
        err = i2c_master_transmit(dev, &reg, 1, 100);
        if (err == ESP_OK) {
            err = i2c_master_receive(dev, buf, (size_t)count, 100);
        }
    } else {
        err = i2c_master_transmit_receive(dev, &reg, 1, buf, (size_t)count, 100);
    }
    if (err == ESP_OK) {
        printf("0x%02x reg 0x%02x%s:", (unsigned)cfg.device_address, reg,
               split ? " (split)" : "");
        for (int i = 0; i < count; i++) {
            printf(" %02x", buf[i]);
        }
        printf("\n");
    } else {
        printf("read failed: %s\n", esp_err_to_name(err));
        printf("  ACKed the scan but refused this transaction -- try `split`, or\n"
               "  `i2craw 0x%02x` if it has no register pointer at all\n",
               (unsigned)cfg.device_address);
    }
    (void)i2c_master_bus_rm_device(dev);
    return (err == ESP_OK) ? 0 : 1;
}

/*
 * Plain read with no pointer write, for a device that NAKs the register byte.
 * Distinguishes "no register-pointer protocol" from "phantom ACK": a phantom
 * from partial address decoding fails here too.
 */
static int cmd_i2craw(int argc, char **argv)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    i2c_master_dev_handle_t dev = NULL;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,
    };
    uint8_t buf[16] = {0};
    int count;
    esp_err_t err;

    if (argc < 2) {
        printf("usage: i2craw <addr> [count 1..%d]\n", (int)sizeof(buf));
        return 1;
    }
    if (bus == NULL) {
        printf("I2C bus not up -- bsp_i2c_init() has not run\n");
        return 1;
    }

    cfg.device_address = (uint16_t)strtoul(argv[1], NULL, 0);
    count = (argc > 2) ? atoi(argv[2]) : 4;

    if (cfg.device_address < 0x08U || cfg.device_address > 0x77U) {
        printf("address must be 0x08..0x77\n");
        return 1;
    }
    if (count < 1 || count > (int)sizeof(buf)) {
        printf("count must be 1..%d\n", (int)sizeof(buf));
        return 1;
    }

    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) {
        printf("could not add device 0x%02x\n", (unsigned)cfg.device_address);
        return 1;
    }
    err = i2c_master_receive(dev, buf, (size_t)count, 100);
    if (err == ESP_OK) {
        printf("0x%02x raw:", (unsigned)cfg.device_address);
        for (int i = 0; i < count; i++) {
            printf(" %02x", buf[i]);
        }
        printf("\n");
    } else {
        printf("raw read failed: %s\n", esp_err_to_name(err));
        printf("  ACKs its address but yields no data on either transaction shape --\n"
               "  likely a partial-address-decode phantom, or write-only\n");
    }
    (void)i2c_master_bus_rm_device(dev);
    return (err == ESP_OK) ? 0 : 1;
}

/*
 * Sweep one device's register space, read-only, to recover an undocumented map.
 * The SW6106 datasheet names exactly one register (0xb0) and publishes no map,
 * so the only way to find the fuel gauge is to look at which registers hold
 * something other than 00/ff.
 *
 * Reads only. Some devices clear interrupt flags on read, so a dump is not
 * perfectly side-effect free -- but it cannot reconfigure a battery charger,
 * which a blind write absolutely can.
 */
static int cmd_i2cdump(int argc, char **argv)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    i2c_master_dev_handle_t dev = NULL;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,
    };
    unsigned start;
    unsigned end;
    int interesting = 0;

    if (argc < 2) {
        printf("usage: i2cdump <addr> [start] [end]   (default 0x00..0xff)\n");
        printf("  read-only. '--' = NAK, '..' = reads 00, 'ff' shown as-is.\n");
        return 1;
    }
    if (bus == NULL) {
        printf("I2C bus not up -- bsp_i2c_init() has not run\n");
        return 1;
    }

    cfg.device_address = (uint16_t)strtoul(argv[1], NULL, 0);
    start = (argc > 2) ? (unsigned)strtoul(argv[2], NULL, 0) : 0x00U;
    end   = (argc > 3) ? (unsigned)strtoul(argv[3], NULL, 0) : 0xffU;

    if (cfg.device_address < 0x08U || cfg.device_address > 0x77U) {
        printf("address must be 0x08..0x77\n");
        return 1;
    }
    if (start > 0xffU || end > 0xffU || start > end) {
        printf("range must be 0x00..0xff and start <= end\n");
        return 1;
    }
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) {
        printf("could not add device 0x%02x\n", (unsigned)cfg.device_address);
        return 1;
    }

    printf("dump 0x%02x regs 0x%02x..0x%02x\n",
           (unsigned)cfg.device_address, start, end);
    printf("      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    for (unsigned base = start & 0xf0U; base <= end; base += 0x10U) {
        printf("%02x:  ", base);
        for (unsigned lo = 0U; lo < 0x10U; lo++) {
            unsigned r = base | lo;
            uint8_t reg = (uint8_t)r;
            uint8_t val = 0U;

            if (r < start || r > end) {
                printf("   ");
                continue;
            }
            if (i2c_master_transmit_receive(dev, &reg, 1, &val, 1, 100) != ESP_OK) {
                printf("-- ");
            } else if (val == 0U) {
                printf(".. ");
            } else {
                printf("%02x ", val);
                interesting++;
            }
        }
        printf("\n");
    }
    printf("\n%d register(s) read non-zero\n\n", interesting);
    (void)i2c_master_bus_rm_device(dev);
    return 0;
}

static int cmd_stats(int argc, char **argv)
{
    /* Time the SVM over many runs: one call is far below esp_timer's
     * resolution, so a single measurement would just read 0 or 1 us. */
    const int iterations = 1000;
    vitals_t v = {
        .hr = 112, .spo2 = 93, .rr = 24, .bp_sys = 100, .bp_dia = 65,
        .battery = 80, .valid = true,
    };
    float conf = 0.0f;
    int64_t t0;
    int64_t elapsed_us;
    /* esp_lvgl_port names it "taskLVGL", not "LVGL". */
    TaskHandle_t lvgl = xTaskGetHandle("taskLVGL");

    (void)argc;
    (void)argv;

    t0 = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        (void)tb_svm_classify(&v, &conf);
    }
    elapsed_us = esp_timer_get_time() - t0;

    printf("\n--- inference ---\n");
    printf("tb_svm_classify: %.2f us/call (%d calls in %lld us)\n",
           (double)elapsed_us / iterations, iterations, elapsed_us);
    printf("called once per patient, so ~%.4f%% of one 60 s measure window\n",
           100.0 * ((double)elapsed_us / iterations) / 60e6);

    printf("\n--- heap ---\n");
    printf("internal free  : %u bytes (min ever %u)\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    printf("PSRAM free     : %u bytes (min ever %u)\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    printf("largest block  : %u internal / %u PSRAM\n",
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

    printf("\n--- task stacks (bytes still unused; 0 means overflow) ---\n");
    if (lvgl != NULL) {
        /* This is the one that overflowed at the default 7168 during bring-up. */
        printf("taskLVGL : %u of 32768\n",
               (unsigned)(uxTaskGetStackHighWaterMark(lvgl) * sizeof(StackType_t)));
    } else {
        printf("taskLVGL : not found\n");
    }
    TaskHandle_t rx = xTaskGetHandle("tb_rx");
    if (rx != NULL) {
        printf("tb_rx    : %u of 4096\n",
               (unsigned)(uxTaskGetStackHighWaterMark(rx) * sizeof(StackType_t)));
    }

    printf("\n--- link ---\n");
    printf("polls_ok=%u polls_failed=%u btn_dropped=%u\n",
           (unsigned)tb_link_frames_ok(), (unsigned)tb_link_crc_errors(),
           (unsigned)tb_ui_source_buttons_dropped());
    printf("\n");
    return 0;
}

/*
 * Read and decode the STM32's whole snapshot in one shot. `i2creg 0x42 0 48`
 * would show the same bytes, but reading hex and applying tb_regs.h offsets by
 * hand is exactly where mistakes happen -- and this also proves the ESP32's
 * decode path agrees with the STM32's layout, which raw hex cannot.
 *
 * Read-only, like every other I2C command here.
 */
static int cmd_i2clink(int argc, char **argv)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    i2c_master_dev_handle_t dev = NULL;
    uint8_t raw[TB_REG_READ_END];
    uint8_t reg = TB_REG_PROTO_VER;
    vitals_t v;
    esp_err_t err;
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TB_I2C_SLAVE_ADDR,
        .scl_speed_hz = 100000,
    };

    (void)argc;
    (void)argv;

    if (bus == NULL) {
        printf("i2c bus not up\n");
        return 1;
    }
    /* Own handle rather than reusing the poll task's: this must work even if
     * tb_link_start() failed, which is precisely when it is most useful. */
    err = i2c_master_bus_add_device(bus, &cfg, &dev);
    if (err != ESP_OK) {
        printf("add_device failed: %s\n", esp_err_to_name(err));
        return 1;
    }

    err = i2c_master_transmit_receive(dev, &reg, 1, raw, sizeof(raw), 200);
    if (err != ESP_OK) {
        printf("read failed: %s\n", esp_err_to_name(err));
        printf("  check SDA=GPIO15->PB3, SCL=GPIO7->PB10, and that the STM32\n"
               "  is running (not halted at a breakpoint).\n");
        (void)i2c_master_bus_rm_device(dev);
        return 1;
    }

    printf("proto_ver : 0x%02x %s\n", raw[TB_REG_PROTO_VER],
           (raw[TB_REG_PROTO_VER] == TB_PROTO_VER) ? "(ok)"
                                                   : "(MISMATCH -- tb_regs.h "
                                                     "differs from the STM32)");
    printf("seq       : %u   (must change between calls, or the superloop is stuck)\n",
           raw[TB_REG_SEQ]);
    printf("flags     : 0x%02x  hr=%d spo2=%d rr=%d bp=%d measuring=%d\n",
           raw[TB_REG_FLAGS],
           (raw[TB_REG_FLAGS] & TB_FLAG_HR_VALID) ? 1 : 0,
           (raw[TB_REG_FLAGS] & TB_FLAG_SPO2_VALID) ? 1 : 0,
           (raw[TB_REG_FLAGS] & TB_FLAG_RR_VALID) ? 1 : 0,
           (raw[TB_REG_FLAGS] & TB_FLAG_BP_VALID) ? 1 : 0,
           (raw[TB_REG_FLAGS] & TB_FLAG_MEASURING) ? 1 : 0);
    printf("buttons   : 0x%02x  [%c%c%c%c]  (1=pressed, left to right)\n",
           raw[TB_REG_BUTTONS],
           (raw[TB_REG_BUTTONS] & TB_BTN_1) ? '1' : '.',
           (raw[TB_REG_BUTTONS] & TB_BTN_2) ? '2' : '.',
           (raw[TB_REG_BUTTONS] & TB_BTN_3) ? '3' : '.',
           (raw[TB_REG_BUTTONS] & TB_BTN_4) ? '4' : '.');
    printf("sensor_ok : 0x%02x  ecg=%d max30102=%d mic=%d rfid=%d lora=%d\n",
           raw[TB_REG_SENSOR_OK],
           (raw[TB_REG_SENSOR_OK] & TB_SENSOR_ECG) ? 1 : 0,
           (raw[TB_REG_SENSOR_OK] & TB_SENSOR_MAX30102) ? 1 : 0,
           (raw[TB_REG_SENSOR_OK] & TB_SENSOR_MIC) ? 1 : 0,
           (raw[TB_REG_SENSOR_OK] & TB_SENSOR_RFID) ? 1 : 0,
           (raw[TB_REG_SENSOR_OK] & TB_SENSOR_LORA) ? 1 : 0);
    printf("battery   : %u%s\n", raw[TB_REG_BATTERY],
           (raw[TB_REG_BATTERY] == 0xFFU) ? " (not measured)" : "%");

    if (tb_i2c_decode_vitals(raw, &v)) {
        printf("decoded   : hr=%u spo2=%u rr=%u bp=%u/%u valid=%d\n",
               v.hr, v.spo2, v.rr, v.bp_sys, v.bp_dia, (int)v.valid);
    } else {
        printf("decoded   : REFUSED (proto_ver mismatch)\n");
    }

    if (raw[TB_REG_RFID_LEN] > 0U) {
        uint8_t n = raw[TB_REG_RFID_LEN];
        if (n > TB_RFID_MAX) {
            n = TB_RFID_MAX;
        }
        printf("rfid      : %u bytes \"%.*s\"\n", raw[TB_REG_RFID_LEN], (int)n,
               (const char *)&raw[TB_REG_RFID]);
    } else {
        printf("rfid      : none\n");
    }

    (void)i2c_master_bus_rm_device(dev);
    return 0;
}

void tb_debug_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_usb_serial_jtag_config_t dev_cfg =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

    repl_cfg.prompt = "triagebox>";
    repl_cfg.max_cmdline_length = 128;

    static const esp_console_cmd_t cmds[] = {
        {.command = "rfid",   .help = "Inject RFID frame [tag]",                 .func = cmd_rfid},
        {.command = "vital",  .help = "Inject VITAL [hr spo2 rr sys dia]",       .func = cmd_vital},
        {.command = "status", .help = "Inject STATUS [sensor_mask] [lora_ok]",   .func = cmd_status},
        {.command = "btn",    .help = "Press button 0..3",                       .func = cmd_btn},
        {.command = "i2c",    .help = "Scan shared I2C bus [timeout_ms]",        .func = cmd_i2c},
        {.command = "i2creg", .help = "Read regs: i2creg <addr> <reg> [count] [split]", .func = cmd_i2creg},
        {.command = "i2craw", .help = "Read with no reg pointer: i2craw <addr> [count]", .func = cmd_i2craw},
        {.command = "i2cdump", .help = "Dump reg space: i2cdump <addr> [start] [end]", .func = cmd_i2cdump},
        {.command = "i2clink", .help = "Read + decode the STM32 snapshot at 0x42", .func = cmd_i2clink},
        {.command = "stats",  .help = "CPU/heap/stack report",                   .func = cmd_stats},
    };

    if (esp_console_new_repl_usb_serial_jtag(&dev_cfg, &repl_cfg, &repl) != ESP_OK) {
        ESP_LOGE(TAG, "console init failed");
        return;
    }
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGW(TAG, "debug console ON (rfid/vital/status/btn/i2c*/stats) -- disable for production");
}
