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
static uint8_t s_idis_l;       /* REG 0x19 */
static uint8_t s_idis_h;       /* REG 0x18 */
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
static bool s_lock_ok;          /* bsp_i2c_lock() result */
static int s_locks;             /* takes, to prove every path locks */
static int s_unlocks;           /* gives, to prove every path unlocks */
static bool s_expander_ok;      /* bsp_io_expander_init() finds the TCA9554 */
static int s_exp_fail_at;       /* fail the Nth expander write (0 = never) */
static int s_exp_txn;

const char *esp_err_to_name(esp_err_t err) { (void)err; return "ESP_ERR_FAKE"; }
void vTaskDelay(int ticks) { (void)ticks; s_waited = true; }

bool bsp_i2c_lock(uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (!s_lock_ok) {
        return false;
    }
    s_locks++;
    return true;
}

void bsp_i2c_unlock(void) { s_unlocks++; }

esp_io_expander_handle_t bsp_io_expander_init(void)
{
    return s_expander_ok ? (esp_io_expander_handle_t)3 : NULL;
}

esp_err_t esp_io_expander_set_dir(esp_io_expander_handle_t h, uint32_t m, int d)
{ (void)h; (void)m; (void)d; return (++s_exp_txn == s_exp_fail_at) ? -1 : ESP_OK; }
esp_err_t esp_io_expander_set_level(esp_io_expander_handle_t h, uint32_t m, uint8_t l)
{ (void)h; (void)m; (void)l; return (++s_exp_txn == s_exp_fail_at) ? -1 : ESP_OK; }

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
    case 0x19U: *r = s_idis_l; break; /* Idischg [7:0]  */
    case 0x18U: *r = s_idis_h; break; /* Idischg [11:8] + Ichg [3:0] */
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
    s_idis_l = 0;
    s_idis_h = 0;
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
    s_lock_ok = true;
    s_locks = 0;
    s_unlocks = 0;
    s_expander_ok = true;
    s_exp_fail_at = 0;
    s_exp_txn = 0;
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

    /*
     * Bus wedged or held by another task: same answer as an unreadable gauge --
     * false, outputs untouched -- and NOT a block. A stale percentage on a
     * draining pack is the one failure mode this must never produce, and a
     * portMAX_DELAY here would freeze whichever task asked.
     */
    reset(0xfaU, 0);
    s_lock_ok = false;
    s_soc = 55U;
    assert(!ui_board_battery(&pct, &chg));
    assert(pct == 7 && chg);
    assert(s_txn == 0);         /* refused before touching the bus */
    assert(s_unlocks == 0);     /* nothing taken, so nothing to give back */

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

    /*
     * Out of range is NO reading, not a clamp. This is the assert that matters:
     * 0xff is what a floating bus or a NAK reads back, and the old code masked
     * bit 7 away, clamped 127 to 100 and returned success -- so a dead gauge
     * displayed a FULL battery, and UI_BATTERY_UNKNOWN (0xff) could never reach
     * ui_status_battery_text() at all. Lying upward about the pack is the one
     * direction that matters on a device that has to last a shift.
     */
    reset(0xfaU, 0);
    s_soc = 0xFFU;
    assert(!ui_board_battery(&pct, &chg));
    assert(pct == 60U);         /* still the previous reading, not 100 "full" */

    reset(0xfaU, 0);
    s_soc = 0x80U | 101U;       /* first value past the spec'd range */
    assert(!ui_board_battery(&pct, &chg));
    assert(pct == 60U);

    reset(0xfaU, 0);
    s_soc = 100U;               /* and the last one inside it still works */
    assert(ui_board_battery(&pct, &chg));
    assert(pct == 100U);

    /* Both outputs optional. */
    reset(0xfaU, 0);
    s_soc = 30U;
    assert(ui_board_battery(NULL, NULL));
    /* Balanced: a leaked take would wedge the bus for every other user, which is
     * the fault this lock was added to fix. */
    assert(s_locks == 1 && s_unlocks == 1);
}

/*
 * ui_board_init() must never abort. The three expander writes are the first I2C
 * of boot, and with PANIC_PRINT_REBOOT + a 0 s delay an ESP_ERROR_CHECK there is
 * an endless reboot with the backlight left lit -- which is exactly the
 * "blackscreen, backlight menyala" report, and reachable for real because the
 * STM32 has been measured holding SCL low after GT911 init.
 */
static void test_init_degrades(void)
{
    reset(0xfaU, 0);
    s_bus_ok = false;
    ui_board_init();            /* no bus: returns, takes no lock */
    assert(s_locks == 0 && s_unlocks == 0);

    reset(0xfaU, 0);
    s_lock_ok = false;
    ui_board_init();
    assert(s_unlocks == 0);

    reset(0xfaU, 0);
    s_expander_ok = false;      /* TCA9554 absent */
    ui_board_init();
    assert(s_locks == 1 && s_unlocks == 1);

    /* Each of the three writes failing in turn: logged, lock returned, and
     * s_io cleared so backlight/buzzer become no-ops instead of half-driving an
     * expander whose direction register never took. */
    for (int nth = 1; nth <= 3; nth++) {
        reset(0xfaU, 0);
        s_exp_fail_at = nth;
        ui_board_init();
        assert(s_locks == 1 && s_unlocks == 1);

        s_exp_txn = 0;
        ui_board_backlight(true);
        ui_board_buzzer(true);
        assert(s_exp_txn == 0); /* s_io == NULL, so nothing was driven */
        assert(s_locks == 1);   /* and no lock was taken to do nothing */
    }

    /* Happy path, last: it leaves s_io set for the rest of the process. */
    reset(0xfaU, 0);
    ui_board_init();
    assert(s_exp_txn == 3);     /* set_dir, buzzer quiet, backlight on */
    assert(s_locks == 1 && s_unlocks == 1);
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

static void test_battery_ma(void)
{
    uint16_t ma = 1234U;

    /* 1.00 A: Idischg raw 0x1C0 = 448 counts * 25/7 mA = 1600 mA. The HIGH nibble
     * of 0x18 holds Idischg's [11:8]; its low nibble is what the `pmic` command
     * decodes as Ichg's high bits and must not bleed into the discharge result. */
    reset(0xfaU, 0);
    s_idis_l = 0xC0U;
    s_idis_h = 0x10U; /* [7:4] = 0x1 is Idischg [11:8]; [3:0] = 1 is Ichg's */
    assert(ui_board_battery_ma(&ma));
    assert(ma == 1600U);

    /* A light-load spot check: 0x1E0 raw = 480 counts -> 1714 mA. */
    reset(0xfaU, 0);
    s_idis_l = 0xE0U;
    s_idis_h = 0x10U;
    assert(ui_board_battery_ma(&ma));
    assert(ma == 1714U);

    /* Zero discharge: charging. Both halves read 0, giving a true 0 mA rather
     * than a decode error -- the caller is expected to say "charging" from the
     * STATUS bit. */
    reset(0xfaU, 0);
    s_idis_l = 0U;
    s_idis_h = 0U;
    assert(ui_board_battery_ma(&ma));
    assert(ma == 0U);

    /* Either half unreadable: no number rather than half a number. */
    reset(0xfaU, 1);
    assert(!ui_board_battery_ma(&ma));
    assert(ma == 0U); /* output untouched */
    reset(0xfaU, 2);
    assert(!ui_board_battery_ma(&ma));
    assert(ma == 0U);

    /* NULL is accepted, same contract as the other gauges. */
    reset(0xfaU, 0);
    assert(ui_board_battery_ma(NULL));
}

int main(void)
{
    test_battery();
    test_battery_mv();
    test_battery_ma();
    test_init_degrades();

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
    assert(s_locks == 1 && s_unlocks == 1); /* one balanced lock spans it all */

    /*
     * Bus wedged/held before the sequence starts: must NOT power off, must not
     * leave the device handle behind, must not leak a lock it never took.
     * Refusing is recoverable (press again); a half-unlocked PMIC is not.
     */
    reset(0xfaU, 0);
    s_lock_ok = false;
    ui_board_power_off();
    assert(s_writes == 0);
    assert(s_txn == 0);
    assert(s_removed);          /* handle added before the lock, so freed here */
    assert(s_unlocks == 0);

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
    assert(s_locks == 0 && s_unlocks == 0);

    printf("ui_board_power_off: 0x01<-0x40, 0x01<-0x80, 0x03<-0x10 under one "
           "bus lock; every failure path leaves the rail up and the lock free\n");
    printf("ui_board_battery: 0x4F[6:0] + 0x11[4], reads only, no unlock; a busy "
           "bus reads as no reading, never a stale one -- and 101..127 is "
           "refused, not clamped to 100\n");
    printf("ui_board_init: every expander-write failure logs and degrades to "
           "no backlight/buzzer control; no path aborts\n");
    printf("ui_board_battery_mv: ((0x15[3:0]<<8)|0x14) * 1.2mV\n");
    printf("ui_board_battery_ma: ((0x18[7:4]<<8)|0x19) * 25/7 mA, 0 while "
           "charging by design\n");
    return 0;
}
