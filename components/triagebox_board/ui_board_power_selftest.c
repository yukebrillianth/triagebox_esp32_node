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
    assert(w[0] == 0x49U);      /* the gate register, nothing else */
    *r = s_gauge;
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

int main(void)
{
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
    return 0;
}
