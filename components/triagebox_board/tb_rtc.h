#ifndef TB_RTC_H
#define TB_RTC_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * PCF85063A real-time clock at 0x51 on the shared I2C bus.
 *
 * The only clock source this firmware has. There is no WiFi and no SNTP, so
 * nothing else can ever tell it what time it is: if the backup cell is flat or
 * the time was never written, the system clock stays at the epoch and
 * ui_status_format_clock() keeps the status bar showing "--:--". That is the
 * intended end state, not a bug -- a wrong timestamp on every reading the
 * station files is worse than a missing one.
 *
 * The chip holds LOCAL WIB time, not UTC. Deliberate: mktime() with TZ=WIB-7
 * turns a register read straight into an epoch with no hand-rolled calendar
 * arithmetic, and `rtc read` then prints what a wall clock next to the box says.
 * It costs nothing, because Indonesia has never observed DST and so there is no
 * ambiguous hour to resolve.
 */

/*
 * POSIX TZ for Western Indonesia. The sign is INVERTED in this format -- POSIX
 * counts west-positive -- so WIB (UTC+7) really is "WIB-7", and no DST half
 * follows because Indonesia has never observed it. Exported so the selftest
 * asserts against the shipped string rather than its own copy of it: a flipped
 * sign here silently shows every clock 14 hours out.
 */
#define TB_RTC_TZ "WIB-7"

/*
 * Set TZ to WIB, read the chip, and push the result into the system clock with
 * settimeofday(). Call once from app_main after the I2C bus is up (the display
 * brings it up) and before the UI starts, so the first status bar paint already
 * has the time.
 *
 * An unreadable or never-set RTC logs and leaves the clock invalid; it never
 * stops the boot.
 */
void tb_rtc_init(void);

/*
 * The chip's own time, in local WIB, or false when it could not be read or its
 * oscillator-stop flag says the counters cannot be trusted (flat backup cell,
 * first power-up, time never written). *out is untouched on false.
 */
bool tb_rtc_read(struct tm *out);

/*
 * Write local WIB time to the chip, clearing the oscillator-stop flag so a
 * later tb_rtc_read() will accept it. Returns false on a bad field or a failed
 * transaction; it does NOT touch the system clock -- call tb_rtc_init() after a
 * successful write to read it back and apply it.
 *
 * tm_wday is written to the chip's weekday register, but nothing in this
 * firmware reads it back; mktime() fills it in for free.
 */
bool tb_rtc_write(const struct tm *t);

/*
 * BCD <-> binary, non-recursive by design: the classic recursive
 * "bcd += 6 * (bcd >> 4)" one-liner has no intermediate an assert can catch,
 * and both directions live here so a single selftest can pin the pair against
 * each other. Host-runnable on purpose (see tb_rtc_selftest.c).
 */
uint8_t tb_rtc_bcd_to_bin(uint8_t bcd);
uint8_t tb_rtc_bin_to_bcd(uint8_t bin);

#endif /* TB_RTC_H */
