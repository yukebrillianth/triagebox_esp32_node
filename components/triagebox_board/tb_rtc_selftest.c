/*
 * Host check of the PCF85063A driver: the BCD conversion, the range rejection,
 * and the bus lock. The register bytes below are written out by hand from the
 * chip's own encoding, NOT produced by the code under test -- BCD is invisibly
 * wrong for part of the year (September is 0x09 whether or not you convert;
 * October is 0x10 in BCD and 0x0a if you forget), so a test that re-derives the
 * expected bytes proves only that the file agrees with itself.
 *
 * Compiles the real tb_rtc.c against test_fakes/, so these asserts cannot drift
 * away from the shipped code. tb_rtc_init() is deliberately NOT exercised: it
 * calls settimeofday(), and a test that moves the developer's system clock is
 * worse than three untested lines.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fakes.h"
#include "tb_rtc.h"

/* --- fake PCF85063A ------------------------------------------------------- */

static uint8_t s_regs[7];      /* what 0x04.. reads back */
static uint8_t s_written[8];   /* pointer byte + 7 values */
static int s_writes;
static int s_reads;
static bool s_bus_ok;
static bool s_add_ok;
static bool s_txn_ok;
static bool s_lock_ok;
static int s_locks;
static int s_unlocks;
static bool s_removed;
static bool s_wrong_addr;

const char *esp_err_to_name(esp_err_t err) { (void)err; return "ESP_ERR_FAKE"; }
void vTaskDelay(int ticks) { (void)ticks; }

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
    if (cfg->device_address != 0x51U) {
        s_wrong_addr = true;    /* 0x3c is the PMIC; a stray write there is a
                                 * power cut, so the address is worth pinning */
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
    /* One pointer byte, then the whole time block in one go: seven separate
     * reads would tear across a minute rollover on a bus the GT911 shares. */
    assert(wlen == 1U && w[0] == 0x04U);
    assert(rlen == sizeof(s_regs));
    assert(s_locks == s_unlocks + 1); /* inside the lock, not around it */
    s_reads++;
    if (!s_txn_ok) {
        return -1;
    }
    memcpy(r, s_regs, sizeof(s_regs));
    return ESP_OK;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t *w,
                              size_t wlen, int timeout_ms)
{
    (void)dev; (void)timeout_ms;
    assert(wlen == sizeof(s_written));
    assert(s_locks == s_unlocks + 1);
    s_writes++;
    if (!s_txn_ok) {
        return -1;
    }
    memcpy(s_written, w, wlen);
    return ESP_OK;
}

static void reset(void)
{
    memset(s_regs, 0, sizeof(s_regs));
    memset(s_written, 0, sizeof(s_written));
    s_writes = 0;
    s_reads = 0;
    s_bus_ok = true;
    s_add_ok = true;
    s_txn_ok = true;
    s_lock_ok = true;
    s_locks = 0;
    s_unlocks = 0;
    s_removed = false;
    s_wrong_addr = false;
}

/* Load 0x04..0x0a. Callers spell the bytes as the chip holds them. */
static void chip_says(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day,
                      uint8_t wday, uint8_t mon, uint8_t year)
{
    reset();
    s_regs[0] = sec;
    s_regs[1] = min;
    s_regs[2] = hour;
    s_regs[3] = day;
    s_regs[4] = wday;
    s_regs[5] = mon;
    s_regs[6] = year;
}

/* --- BCD, both directions ------------------------------------------------- */

static void test_bcd(void)
{
    /* Hand-written pairs. The interesting ones are the values where BCD and
     * binary diverge, i.e. every byte whose low nibble would overflow 9. */
    assert(tb_rtc_bcd_to_bin(0x00U) == 0U);
    assert(tb_rtc_bcd_to_bin(0x09U) == 9U);
    assert(tb_rtc_bcd_to_bin(0x10U) == 10U);   /* NOT 16 */
    assert(tb_rtc_bcd_to_bin(0x12U) == 12U);   /* December */
    assert(tb_rtc_bcd_to_bin(0x23U) == 23U);   /* last hour of the day */
    assert(tb_rtc_bcd_to_bin(0x31U) == 31U);   /* last day of a long month */
    assert(tb_rtc_bcd_to_bin(0x59U) == 59U);   /* last second of a minute */
    assert(tb_rtc_bcd_to_bin(0x99U) == 99U);   /* 2099 */

    assert(tb_rtc_bin_to_bcd(0U) == 0x00U);
    assert(tb_rtc_bin_to_bcd(9U) == 0x09U);
    assert(tb_rtc_bin_to_bcd(10U) == 0x10U);   /* the one that catches "October
                                                * went out as 0x0a" */
    assert(tb_rtc_bin_to_bcd(12U) == 0x12U);
    assert(tb_rtc_bin_to_bcd(23U) == 0x23U);
    assert(tb_rtc_bin_to_bcd(31U) == 0x31U);
    assert(tb_rtc_bin_to_bcd(59U) == 0x59U);
    assert(tb_rtc_bin_to_bcd(99U) == 0x99U);

    /* Round trip over every value the registers can hold. Cheap, and it is the
     * assert that survives someone "optimising" either direction. */
    for (uint8_t v = 0U; v <= 99U; v++) {
        assert(tb_rtc_bcd_to_bin(tb_rtc_bin_to_bcd(v)) == v);
    }
}

/* --- reads ---------------------------------------------------------------- */

static void test_read(void)
{
    struct tm t;

    /* The chip's own answer, rendered the way the firmware renders it: mktime()
     * under the shipped TZ string. The literal is date(1)'s word for
     * 2026-09-05 07:30:07 UTC, which is 14:30:07 WIB -- the standard library
     * computes the calendar, the driver only has to decode the right date. */
    assert(setenv("TZ", TB_RTC_TZ, 1) == 0);
    tzset();
    chip_says(0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    assert(tb_rtc_read(&t));
    assert(mktime(&t) == (time_t)1788593407);

    /* No bus, and cannot address: false, and no transaction attempted. Ordered
     * first because these are the paths a cached device handle would hide. */
    reset();
    s_bus_ok = false;
    assert(!tb_rtc_read(&t));
    assert(s_reads == 0 && s_locks == 0);

    reset();
    s_add_ok = false;
    assert(!tb_rtc_read(&t));
    assert(s_reads == 0 && s_locks == 0 && !s_wrong_addr);

    /*
     * 2026-09-05 14:30:07, a Saturday (weekday 6). Every byte is the decimal
     * number written in hex, which is what BCD means: minute 30 is 0x30.
     */
    chip_says(0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    memset(&t, 0xff, sizeof(t));
    assert(tb_rtc_read(&t));
    assert(t.tm_sec == 7 && t.tm_min == 30 && t.tm_hour == 14);
    assert(t.tm_mday == 5 && t.tm_wday == 6);
    assert(t.tm_mon == 8);            /* struct tm counts months from 0 */
    assert(t.tm_year == 126);         /* and years from 1900 */
    assert(t.tm_isdst == 0);          /* WIB has none; -1 would ask libc to guess */
    assert(s_locks == 1 && s_unlocks == 1 && s_removed);

    /* The month that catches a missing conversion: October is 0x10, and a driver
     * that wrote/read plain binary would call this one October too -- so the
     * paired assert below is the one that matters. */
    chip_says(0x00U, 0x00U, 0x00U, 0x01U, 0x04U, 0x10U, 0x26U);
    assert(tb_rtc_read(&t));
    assert(t.tm_mon == 9);
    /* 0x0a in the months register is not a month at all. A driver that skipped
     * bin_to_bcd would put that byte there, and a lenient reader would decode it
     * back to 10 and never notice. */
    chip_says(0x00U, 0x00U, 0x00U, 0x01U, 0x04U, 0x0aU, 0x26U);
    assert(!tb_rtc_read(&t));

    /* Leap day, because February is where a wrong day register shows up:
     * 2024-02-29 was a Thursday (weekday 4). */
    chip_says(0x00U, 0x00U, 0x12U, 0x29U, 0x04U, 0x02U, 0x24U);
    assert(tb_rtc_read(&t));
    assert(t.tm_year == 124 && t.tm_mon == 1 && t.tm_mday == 29);

    /* Both extremes of the year register, so nobody "simplifies" the +100. */
    chip_says(0x59U, 0x59U, 0x23U, 0x31U, 0x05U, 0x12U, 0x99U);
    assert(tb_rtc_read(&t));
    assert(t.tm_year == 199 && t.tm_mon == 11 && t.tm_mday == 31);
    assert(t.tm_hour == 23 && t.tm_min == 59 && t.tm_sec == 59);
    chip_says(0x00U, 0x00U, 0x00U, 0x01U, 0x06U, 0x01U, 0x00U);
    assert(tb_rtc_read(&t));
    assert(t.tm_year == 100);

    /* Unused bits are not data: hours[7:6], days[7:6], minutes[7] and weekday's
     * top bits read as whatever the part leaves there. */
    chip_says(0x07U, 0x30U, 0xD4U, 0xC5U, 0xF6U, 0xE9U, 0x26U);
    assert(tb_rtc_read(&t));
    assert(t.tm_hour == 14 && t.tm_mday == 5 && t.tm_wday == 6 && t.tm_mon == 8);

    /*
     * Oscillator-stop flag in Seconds[7]: the counters are a stale guess, so
     * there is no time here at all. This is the state a freshly fitted backup
     * cell is in, and the reason the clock has to be settable.
     */
    chip_says(0x80U | 0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    assert(!tb_rtc_read(&t));

    /* Floating bus / NAK-as-data. Both must read as "no time", never as a date:
     * a normalizing mktime() would otherwise turn them into a confident wrong
     * clock instead of leaving the bar at "--:--". */
    chip_says(0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU);
    assert(!tb_rtc_read(&t));
    chip_says(0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U);
    assert(!tb_rtc_read(&t)); /* day 0 and month 0 are not a date */

    /* Out of range in each field that has one, one at a time. */
    chip_says(0x60U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U); /* second 60 */
    assert(!tb_rtc_read(&t));
    chip_says(0x07U, 0x60U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U); /* minute 60 */
    assert(!tb_rtc_read(&t));
    chip_says(0x07U, 0x30U, 0x24U, 0x05U, 0x06U, 0x09U, 0x26U); /* hour 24 */
    assert(!tb_rtc_read(&t));
    chip_says(0x07U, 0x30U, 0x14U, 0x32U, 0x06U, 0x09U, 0x26U); /* day 32 */
    assert(!tb_rtc_read(&t));
    chip_says(0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x13U, 0x26U); /* month 13 */
    assert(!tb_rtc_read(&t));

    /* Nibble above 9 is not BCD even when the byte's value looks sane: 0x1f
     * decodes to 25 under a lenient reader. */
    chip_says(0x1fU, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    assert(!tb_rtc_read(&t));

    /* Bus held by the LVGL touch poll or the link task: no reading, and no
     * blocking wait either -- the caller renders "--:--" for one tick. */
    chip_says(0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    s_lock_ok = false;
    assert(!tb_rtc_read(&t));
    assert(s_reads == 0 && s_unlocks == 0 && s_removed);

    /* Failed transaction: false, and the lock still handed back. */
    chip_says(0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    s_txn_ok = false;
    assert(!tb_rtc_read(&t));
    assert(s_locks == 1 && s_unlocks == 1 && s_removed);

    /* NULL out is not a crash. */
    chip_says(0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    assert(!tb_rtc_read(NULL));

    /* On any refusal the caller's struct is untouched, so a periodic reader
     * cannot be talked into showing half of an old timestamp. */
    chip_says(0x07U, 0x30U, 0x14U, 0x05U, 0x06U, 0x09U, 0x26U);
    assert(tb_rtc_read(&t));
    chip_says(0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU);
    assert(!tb_rtc_read(&t));
    assert(t.tm_hour == 14 && t.tm_min == 30);
}

/* --- writes --------------------------------------------------------------- */

static struct tm at(int year, int mon, int mday, int hour, int min, int sec)
{
    struct tm t = {0};

    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    return t;
}

static void test_write(void)
{
    struct tm t;

    /*
     * 2027-10-09 23:59:58, a Saturday. Bytes spelled out by hand: pointer 0x04,
     * then seconds .. years. October is the byte this file exists for.
     */
    reset();
    t = at(2027, 10, 9, 23, 59, 58);
    assert(tb_rtc_write(&t));
    assert(s_writes == 1);
    assert(s_written[0] == 0x04U);   /* Seconds: the block starts there */
    assert(s_written[1] == 0x58U);
    assert(s_written[2] == 0x59U);
    assert(s_written[3] == 0x23U);
    assert(s_written[4] == 0x09U);
    assert(s_written[5] == 0x06U);   /* weekday, filled in by mktime for us */
    assert(s_written[6] == 0x10U);   /* October, not 0x0a */
    assert(s_written[7] == 0x27U);
    /* Seconds[7] clear is what tells the chip its time is trustworthy again --
     * the same bit whose being set makes tb_rtc_read() refuse. */
    assert((s_written[1] & 0x80U) == 0U);
    assert(s_locks == 1 && s_unlocks == 1 && s_removed && !s_wrong_addr);

    /* Round trip through the driver, both directions, one date: the pair of
     * conversions cannot both be wrong in the same direction and still land
     * here. 2025-12-31 was a Wednesday. */
    reset();
    t = at(2025, 12, 31, 7, 5, 3);
    assert(tb_rtc_write(&t));
    assert(s_written[6] == 0x12U && s_written[7] == 0x25U);
    memcpy(s_regs, &s_written[1], sizeof(s_regs));
    assert(tb_rtc_read(&t));
    assert(t.tm_year == 125 && t.tm_mon == 11 && t.tm_mday == 31);
    assert(t.tm_hour == 7 && t.tm_min == 5 && t.tm_sec == 3 && t.tm_wday == 3);

    /*
     * Refusals. The console parses operator text straight into this, so a date
     * that is not a date must never reach the chip half-encoded.
     */
    reset();
    t = at(2026, 2, 30, 12, 0, 0);   /* February has no 30th */
    assert(!tb_rtc_write(&t));
    assert(s_writes == 0);

    reset();
    t = at(2026, 9, 5, 25, 61, 0);   /* hour 25, minute 61 */
    assert(!tb_rtc_write(&t));
    assert(s_writes == 0);

    reset();
    t = at(1999, 12, 31, 23, 59, 59); /* two BCD digits cannot hold 1999 */
    assert(!tb_rtc_write(&t));
    assert(s_writes == 0);

    reset();
    t = at(2100, 1, 1, 0, 0, 0);      /* nor 2100 */
    assert(!tb_rtc_write(&t));
    assert(s_writes == 0);

    reset();
    assert(!tb_rtc_write(NULL));
    assert(s_writes == 0);

    /* Bus busy and a failed transaction: false, nothing left locked. */
    reset();
    s_lock_ok = false;
    t = at(2026, 9, 5, 14, 30, 0);
    assert(!tb_rtc_write(&t));
    assert(s_writes == 0 && s_unlocks == 0 && s_removed);

    reset();
    s_txn_ok = false;
    assert(!tb_rtc_write(&t));
    assert(s_locks == 1 && s_unlocks == 1 && s_removed);

    /* No bus / cannot address: refused before any encoding is trusted. */
    reset();
    s_bus_ok = false;
    assert(!tb_rtc_write(&t));
    reset();
    s_add_ok = false;
    assert(!tb_rtc_write(&t));
    assert(s_writes == 0 && s_locks == 0);
}

int main(void)
{
    /*
     * The offset the whole clock hangs off. Asserted as a literal because the
     * inverted POSIX sign is the mistake to catch: "WIB+7" compiles, runs, and
     * shows every timestamp 14 hours out.
     */
    assert(strcmp(TB_RTC_TZ, "WIB-7") == 0);

    test_bcd();
    test_read();
    test_write();

    printf("tb_rtc: BCD both ways against hand-written register bytes "
           "(October = 0x10), 0x04..0x0a in one locked transaction\n");
    printf("tb_rtc_read: refuses the oscillator-stop flag, a floating bus and "
           "every out-of-range field -- so the bar shows \"--:--\", never a "
           "fabricated time\n");
    printf("tb_rtc_write: clears Seconds[7], fills the weekday, refuses "
           "Feb 30 / 25:61 / 1999 / 2100 before touching the chip\n");
    return 0;
}
