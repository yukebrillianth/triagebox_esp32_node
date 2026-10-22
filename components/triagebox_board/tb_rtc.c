/*
 * PCF85063A RTC driver: the only clock this firmware has. See tb_rtc.h for why
 * the chip holds local WIB time and why an unreadable RTC is a logged skip
 * rather than a boot failure.
 *
 * Transaction shape: the time block is Seconds 0x04 .. Years 0x0A and the
 * address pointer auto-increments across it (unlike the TCA9554 and the SW6106
 * on this same bus, where a burst re-reads one register), so a whole timestamp
 * moves in ONE transaction under one bsp_i2c_lock() -- taken here, not by the
 * caller, exactly like sw6106_read()'s callers in ui_board.c.
 *
 * ponytail: a carry landing inside that ~0.7 ms burst can report the minute one
 * ahead (seconds read as :59, then minutes tick before they are shifted out).
 * At HH:MM resolution, read once at boot, that is not worth a read-twice-and-
 * compare loop. Add one if this ever timestamps data rather than a status bar.
 */
#define _DEFAULT_SOURCE /* glibc hides settimeofday/setenv/tzset under -std=c99,
                         * which the host selftest uses; newlib and macOS expose
                         * them anyway. Must precede every include. */

#include "tb_rtc.h"

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "bsp/esp32_s3_touch_lcd_4.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define PCF85063A_ADDR        0x51U
#define PCF85063A_REG_SECONDS 0x04U
#define PCF85063A_OS_BIT      0x80U /* Seconds[7]: oscillator has stopped */
#define RTC_NREGS             7U    /* sec min hour day weekday month year */
/* One bus turn, never a hang: the same budget ui_board.c uses, and for the same
 * reason -- this runs on the LVGL task when the console asks. */
#define RTC_I2C_TIMEOUT_MS    100

static const char *TAG = "tb_rtc";

uint8_t tb_rtc_bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10U + (bcd & 0x0FU));
}

uint8_t tb_rtc_bin_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10U) << 4) | (bin % 10U));
}

/*
 * Both nibbles decimal AND the byte within its field's maximum, in BCD (so 0x59,
 * not 59). A floating bus reads 0xff and a NAKed device reads 0x00: without this
 * the first becomes year 2155 at 99:99 and mktime() would normalize it into a
 * confident wrong time instead of leaving the bar at "--:--".
 */
static bool bcd_ok(uint8_t raw, uint8_t max)
{
    return ((raw & 0x0FU) <= 9U) && ((raw >> 4) <= 9U) && (raw <= max);
}

static i2c_master_dev_handle_t rtc_open(i2c_master_bus_handle_t bus)
{
    i2c_master_dev_handle_t dev = NULL;
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063A_ADDR,
        .scl_speed_hz = 100000,
    };

    /* Added and removed per call rather than cached: this runs once at boot and
     * from the console, so the churn is nothing, and the selftest gets to drive
     * the "no bus" path more than once -- which a cached handle would swallow. */
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "could not address the RTC at 0x%02x", PCF85063A_ADDR);
        return NULL;
    }
    return dev;
}

bool tb_rtc_read(struct tm *out)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    i2c_master_dev_handle_t dev;
    uint8_t reg = PCF85063A_REG_SECONDS;
    uint8_t t[RTC_NREGS];
    esp_err_t err;

    if (bus == NULL || out == NULL) {
        return false;
    }
    dev = rtc_open(bus);
    if (dev == NULL) {
        return false;
    }
    if (!bsp_i2c_lock(RTC_I2C_TIMEOUT_MS)) {
        ESP_LOGD(TAG, "RTC read skipped: I2C bus busy");
        (void)i2c_master_bus_rm_device(dev);
        return false;
    }
    err = i2c_master_transmit_receive(dev, &reg, 1, t, sizeof(t),
                                     RTC_I2C_TIMEOUT_MS);
    bsp_i2c_unlock();
    (void)i2c_master_bus_rm_device(dev);

    if (err != ESP_OK) {
        return false;
    }
    if ((t[0] & PCF85063A_OS_BIT) != 0U) {
        /*
         * The oscillator stopped at some point since the last write, so whatever
         * the counters hold is a stale guess, not a time. Set on a first
         * power-up, and after the backup cell runs flat. Writing Seconds clears
         * it, which is why one `rtc set` arms the clock for good.
         *
         * This is also the branch a floating bus lands in (0xff has bit 7 set),
         * which is the right answer for the wrong reason -- the range checks
         * below would have caught it too.
         */
        ESP_LOGI(TAG, "RTC oscillator-stop flag set: never written, or the "
                      "backup cell ran flat");
        return false;
    }
    /* Unused bits first, so the checks below see only counter bits. This part has
     * NO century flag in Months[7] -- that is the PCF8563, a different chip. */
    t[0] &= 0x7FU;              /* [7] was the OS flag, handled above */
    t[1] &= 0x7FU;
    t[2] &= 0x3FU;              /* hours, 24-hour count */
    t[3] &= 0x3FU;
    t[4] &= 0x07U;              /* weekday counts 0..6 in plain binary, not BCD */
    t[5] &= 0x1FU;

    if (!bcd_ok(t[0], 0x59U) || !bcd_ok(t[1], 0x59U) || !bcd_ok(t[2], 0x23U) ||
        !bcd_ok(t[3], 0x31U) || t[3] == 0U ||
        !bcd_ok(t[5], 0x12U) || t[5] == 0U || !bcd_ok(t[6], 0x99U)) {
        /* Day and month need the lower bound too: a zeroth of January is what a
         * half-written chip reads back, and mktime() would take it. */
        ESP_LOGW(TAG, "RTC registers not a date: %02x %02x %02x %02x %02x %02x %02x",
                 t[0], t[1], t[2], t[3], t[4], t[5], t[6]);
        return false;
    }

    *out = (struct tm){
        .tm_sec = tb_rtc_bcd_to_bin(t[0]),
        .tm_min = tb_rtc_bcd_to_bin(t[1]),
        .tm_hour = tb_rtc_bcd_to_bin(t[2]),
        .tm_mday = tb_rtc_bcd_to_bin(t[3]),
        .tm_wday = t[4],
        .tm_mon = tb_rtc_bcd_to_bin(t[5]) - 1,   /* chip 1..12, struct tm 0..11 */
        .tm_year = 100 + tb_rtc_bcd_to_bin(t[6]), /* chip counts from 2000 */
        .tm_isdst = 0,                            /* WIB has no DST */
    };
    return true;
}

bool tb_rtc_write(const struct tm *t)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    i2c_master_dev_handle_t dev;
    struct tm norm;
    uint8_t buf[1U + RTC_NREGS]; /* register pointer, then Seconds..Years */
    esp_err_t err;

    if (bus == NULL || t == NULL) {
        return false;
    }
    /*
     * mktime() BEFORE the chip is touched, and the result compared field by
     * field: it normalizes, so 2026-02-30 and 25:61 come back as a different
     * wall time and are refused here. The console is a trust boundary and the
     * console can typo. It also fills tm_wday, which is why the encode below
     * reads `norm` -- the caller never has to get the weekday right.
     */
    norm = *t;
    norm.tm_isdst = 0; /* no DST rule to consult under TZ=WIB-7 */
    if (mktime(&norm) == (time_t)-1 || norm.tm_sec != t->tm_sec ||
        norm.tm_min != t->tm_min || norm.tm_hour != t->tm_hour ||
        norm.tm_mday != t->tm_mday || norm.tm_mon != t->tm_mon ||
        norm.tm_year != t->tm_year) {
        ESP_LOGE(TAG, "refusing a time that does not round-trip through mktime");
        return false;
    }
    if (norm.tm_year < 100 || norm.tm_year > 199) {
        /* Two BCD digits, no century. ponytail: 2100 needs a base-year change
         * here and nowhere else. */
        ESP_LOGE(TAG, "RTC holds 2000..2099; %d is out of range",
                 norm.tm_year + 1900);
        return false;
    }

    /* Seconds[7] goes out as 0, which is how the chip is told its time is
     * trustworthy again -- this write clears the oscillator-stop flag that made
     * tb_rtc_read() refuse. */
    buf[0] = PCF85063A_REG_SECONDS;
    buf[1] = tb_rtc_bin_to_bcd((uint8_t)norm.tm_sec);
    buf[2] = tb_rtc_bin_to_bcd((uint8_t)norm.tm_min);
    buf[3] = tb_rtc_bin_to_bcd((uint8_t)norm.tm_hour); /* 24-hour, bit 7 clear */
    buf[4] = tb_rtc_bin_to_bcd((uint8_t)norm.tm_mday);
    buf[5] = (uint8_t)norm.tm_wday & 0x07U;            /* binary, not BCD */
    buf[6] = tb_rtc_bin_to_bcd((uint8_t)(norm.tm_mon + 1));
    buf[7] = tb_rtc_bin_to_bcd((uint8_t)(norm.tm_year - 100));

    dev = rtc_open(bus);
    if (dev == NULL) {
        return false;
    }
    if (!bsp_i2c_lock(RTC_I2C_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "RTC write skipped: I2C bus busy");
        (void)i2c_master_bus_rm_device(dev);
        return false;
    }
    /* All eight bytes in one transaction: a pointer write and a value write that
     * another task could land between is how a clock ends up half set. */
    err = i2c_master_transmit(dev, buf, sizeof(buf), RTC_I2C_TIMEOUT_MS);
    bsp_i2c_unlock();
    (void)i2c_master_bus_rm_device(dev);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RTC write failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void tb_rtc_init(void)
{
    struct timeval tv;
    struct tm now;
    time_t epoch;

    /*
     * The offset lives in tb_rtc.h so the selftest can assert the shipped
     * string: POSIX TZ is west-positive, so WIB (UTC+7) is "WIB-7" and the minus
     * is not a typo. Without this newlib's localtime() is UTC and every clock on
     * the panel reads 7 h early.
     */
    setenv("TZ", TB_RTC_TZ, 1);
    tzset();

    if (!tb_rtc_read(&now)) {
        /* Leave the system clock at the epoch on purpose: there is no SNTP and
         * no WiFi here, so there is no second source to fall back to, and
         * ui_status_format_clock() renders anything before 2025 as "--:--".
         * Inventing a time would put a wrong one on every reading the station
         * files. `rtc set <YYYY-MM-DD> <HH:MM>` on the debug console is the
         * only way in. */
        ESP_LOGW(TAG, "no valid RTC time -- the status bar clock stays \"--:--\"");
        return;
    }
    epoch = mktime(&now);
    if (epoch == (time_t)-1) {
        ESP_LOGE(TAG, "RTC read %04d-%02d-%02d %02d:%02d:%02d, which mktime() "
                      "refuses", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
                 now.tm_hour, now.tm_min, now.tm_sec);
        return;
    }
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    (void)settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "clock set from RTC: %04d-%02d-%02d %02d:%02d WIB",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour,
             now.tm_min);
}
