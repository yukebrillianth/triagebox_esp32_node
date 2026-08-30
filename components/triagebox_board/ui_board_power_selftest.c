/*
 * Host check of ui_board_power_off(): it writes the exact SW6106 sequence from
 * the register list (RG006_1_v1.2) and, on any failure, leaves the rail up.
 *
 * Compiles the real ui_board.c against test_fakes/ rather than a copy, so the
 * asserts cannot drift away from the shipped code. Worth the fakes: this is the
 * one function in the tree that cuts power to a running device.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fakes.h"
#include "ui_board.h"

/* --- fake SW6106 --------------------------------------------------------- */

#define MAX_LOG 8

static uint8_t s_gauge;        /* what REG 0x49 reads back */
static uint8_t s_soc;          /* REG 0x4F */
static uint8_t s_stat;         /* REG 0x11 */
static uint8_t s_vbat_l;       /* REG 0x14 */
static uint8_t s_vbat_h;       /* REG 0x15 */
static int s_fail_at;          /* fail the Nth transaction (0 = never) */
static int s_txn;
static uint8_t s_reg[MAX_LOG];
static uint8_t s_val[MAX_LOG];
static int s_writes;
static bool s_bus_ok;
static bool s_add_ok;
static bool s_removed;
static bool s_waited;
static bool s_wrong_addr;

const char *esp_err_to_name(esp_err_t err) { (void)err; return "ESP_ERR_FAKE"; }
void vTaskDelay(int ticks) { (void)ticks; s_waited = true; }

esp_io_expander_handle_t bsp_io_expander_init(void) { return NULL; }
esp_err_t esp_io_expander_set_dir(esp_io_expander_handle_t h, uint32_t m, int d)
{ (void)h; (void)m; (void)d; return ESP_OK; }
esp_err_t esp_io_expander_set_level(esp_io_expander_handle_t h, uint32_t m, uint8_t l)
{ (void)h; (void)m; (void)l; return ESP_OK; }

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return s_bus_ok ? (i2c_master_bus_handle_t)1 : NULL;
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                   const i2c_device_config_t *cfg,
                                   i2c_master_dev_handle_t *out)
{
    (void)bus;
    if (!s_add_ok) {
        return -1;
    }
    if (cfg->device_address != 0x3cU) {
        s_wrong_addr = true;
        return -1;
    }
    *out = (i2c_master_dev_handle_t)2;
    return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev)
{
    (void)dev;
    s_removed = true;
    return ESP_OK;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t *w,
                                      size_t wlen, uint8_t *r, size_t rlen,
                                      int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    assert(wlen == 1U && rlen == 1U);
    if (++s_txn == s_fail_at) {
        return -1;
    }
    switch (w[0]) {
    case 0x49U: *r = s_gauge;  break; /* power-off gate */
    case 0x4FU: *r = s_soc;    break; /* fuel gauge     */
    case 0x11U: *r = s_stat;   break; /* charge state   */
    case 0x14U: *r = s_vbat_l; break; /* Vbat [7:0]     */
    case 0x15U: *r = s_vbat_h; break; /* Vbat [11:8]    */
    /* Anything else means new code is poking a 4 A charger's registers. */
    default: assert(0); return -1;
    }
    return ESP_OK;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t *w,
                              size_t wlen, int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    assert(wlen == 2U);         /* register + value, never a bare byte */
    if (++s_txn == s_fail_at) {
        return -1;
    }
    assert(s_writes < MAX_LOG);
    s_reg[s_writes] = w[0];
    s_val[s_writes] = w[1];
    s_writes++;
    return ESP_OK;
}

static void reset(uint8_t gauge, int fail_at)
{
    s_gauge = gauge;
    s_soc = 0;
    s_stat = 0;
    s_vbat_l = 0;
    s_vbat_h = 0;
    s_fail_at = fail_at;
    s_txn = 0;
    s_writes = 0;
    memset(s_reg, 0, sizeof(s_reg));
    memset(s_val, 0, sizeof(s_val));
    s_bus_ok = true;
    s_add_ok = true;
    s_removed = false;
    s_waited = false;
    s_wrong_addr = false;
}

/*
 * Reads only, and no unlock: 0x01/0x22 are the registers that can latch the
 * power path off, and the gauge needs neither.
 *
 * Failure cases come first on purpose -- ui_board_battery() caches its device
 * handle on the first success, so once a good read happens the "no bus" branch is
 * unreachable for the rest of the process.
 */
static void test_battery(void)
{
    uint8_t pct = 7;
    bool chg = true;

    reset(0xfaU, 0);
    s_bus_ok = false;
    assert(!ui_board_battery(&pct, &chg));
    /* Outputs untouched on failure, so the caller renders "--%", not 0%. */
    assert(pct == 7 && chg);
    reset(0xfaU, 0);
    s_add_ok = false;
    assert(!ui_board_battery(&pct, &chg));
    assert(pct == 7 && chg);

    /* Gauge unreadable: no percentage at all. */
    reset(0xfaU, 1);
    s_soc = 55U;
    s_stat = 0x10U;
    assert(!ui_board_battery(&pct, &chg));
    assert(pct == 7);

    /* Charge state unreadable: keep the percentage we did get, lose only the
     * bolt. Collapsing both would hide a good reading. */
    reset(0xfaU, 2);
    s_soc = 55U;
    s_stat = 0x10U;
    assert(ui_board_battery(&pct, &chg));
    assert(pct == 55U && !chg);

    /* 0x4F[7] is not part of the percentage. */
    reset(0xfaU, 0);
    s_soc = 0x80U | 42U;
    assert(ui_board_battery(&pct, &chg));
    assert(pct == 42U && !chg);

    reset(0xfaU, 0);
    s_soc = 100U;
    s_stat = 0x10U;             /* [4] charging */
    assert(ui_board_battery(&pct, &chg));
    assert(pct == 100U && chg);

    /* Discharging is not charging, and must not read as one. */
    reset(0xfaU, 0);
    s_soc = 60U;
    s_stat = 0x20U;             /* [5] discharging */
    assert(ui_board_battery(&pct, &chg));
    assert(pct == 60U && !chg);

    /* A bad read clamps instead of showing 127%. */
    reset(0xfaU, 0);
    s_soc = 0x7FU;
    assert(ui_board_battery(&pct, &chg));
    assert(pct == 100U);

    /* Both outputs optional. */
    reset(0xfaU, 0);
    s_soc = 30U;
    assert(ui_board_battery(NULL, NULL));
}

/*
 * Vbat, for the range-test screen. The decode is worth pinning because a wrong
 * mask or a wrong scale produces a number that still looks like a battery
 * voltage: 0x15's high nibble belongs to Vout, and the LSB is 1.2 mV, not 1.
 */
static void test_battery_mv(void)
{
    uint16_t mv = 1234U;

    /* 3.70 V: raw 0xC0A = 3082 counts x 1.2 mV = 3698 mV. The high nibble is
     * loaded with Vout bits that must not leak into the result. */
    reset(0xfaU, 0);
    s_vbat_l = 0x0AU;
    s_vbat_h = 0xFCU; /* [3:0] = 0xC is Vbat; [7:4] = 0xF is Vout's */
    assert(ui_board_battery_mv(&mv));
    assert(mv == 3698U);

    /* A flat-ish pack, and the scale is not 1:1. */
    reset(0xfaU, 0);
    s_vbat_l = 0xB0U;
    s_vbat_h = 0x0AU; /* raw 0xAB0 = 2736 -> 3283 mV */
    assert(ui_board_battery_mv(&mv));
    assert(mv == 3283U);

    /* Either half unreadable: no voltage at all rather than half a number. */
    reset(0xfaU, 1);
    assert(!ui_board_battery_mv(&mv));
    assert(mv == 3283U); /* output untouched, so the screen keeps showing "--" */
    reset(0xfaU, 2);
    assert(!ui_board_battery_mv(&mv));
    assert(mv == 3283U);

    /* NULL is accepted, same as ui_board_battery(). */
    reset(0xfaU, 0);
    assert(ui_board_battery_mv(NULL));
}

int main(void)
{
    test_battery();
    test_battery_mv();

    /* Happy path: unlock 1, unlock 2, then the power-off event -- in order. */
    reset(0xfaU, 0);
    ui_board_power_off();
    assert(!s_wrong_addr);
    assert(s_writes == 3);
    assert(s_reg[0] == 0x01U && s_val[0] == 0x40U);  /* BG ctrl [7:6] = 1 */
    assert(s_reg[1] == 0x01U && s_val[1] == 0x80U);  /* BG ctrl [7:6] = 2 */
    assert(s_reg[2] == 0x03U && s_val[2] == 0x10U);  /* key evt [4]   = 1 */
    assert(s_waited);       /* gives the rail time to drop before complaining */
    assert(s_removed);      /* no leaked device handle */

    /*
     * Gate closed (REG 0x49[3] == 0): must not write at all. Writing the event
     * anyway would be a silent no-op and the UI would look hung.
     */
    reset(0xf2U, 0);
    ui_board_power_off();
    assert(s_writes == 0);
    assert(s_removed);

    /* Gate unreadable: same -- no blind writes to a battery charger. */
    reset(0xfaU, 1);
    ui_board_power_off();
    assert(s_writes == 0);
    assert(s_removed);

    /*
     * Unlock failed. The event must NOT follow: a power-off written without a
     * completed unlock is ignored by the chip, so sending it would report
     * success while the device stays on.
     */
    reset(0xfaU, 2);            /* first unlock fails */
    ui_board_power_off();
    assert(s_writes == 0);

    reset(0xfaU, 3);            /* second unlock fails */
    ui_board_power_off();
    assert(s_writes == 1 && s_reg[0] == 0x01U);
    for (int i = 0; i < s_writes; i++) {
        assert(s_reg[i] != 0x03U);
    }

    /* No bus, and cannot address: return quietly, nothing written or leaked. */
    reset(0xfaU, 0);
    s_bus_ok = false;
    ui_board_power_off();
    assert(s_writes == 0 && !s_removed);

    reset(0xfaU, 0);
    s_add_ok = false;
    ui_board_power_off();
    assert(s_writes == 0 && !s_removed);

    printf("ui_board_power_off: 0x01<-0x40, 0x01<-0x80, 0x03<-0x10; "
           "every failure path leaves the rail up\n");
    printf("ui_board_battery: 0x4F[6:0] + 0x11[4], reads only, no unlock\n");
    printf("ui_board_battery_mv: ((0x15[3:0]<<8)|0x14) * 1.2mV\n");
    return 0;
}
