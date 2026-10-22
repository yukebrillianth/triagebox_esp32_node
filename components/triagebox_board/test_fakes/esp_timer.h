/*
 * Host stand-in for esp_timer.h: tb_ui_source.c's measure-to-verdict timing log
 * reads a clock the LVGL task cannot stall, and the selftest compiles that real
 * file. Time here is a fake but MONOTONIC -- the test advances it between
 * ui_mock_tick() calls, because a frozen clock would exercise nothing.
 *
 * Test-only -- never in a firmware build.
 */
#ifndef TB_TEST_FAKES_ESP_TIMER_H
#define TB_TEST_FAKES_ESP_TIMER_H

#include <stdint.h>

/* Microseconds since boot. Settable from the test; defaults to a nonzero value
 * so a test that forgets to advance it still exercises the subtraction. */
int64_t tb_fake_time_us(void);
void tb_fake_time_set(int64_t us);

static inline int64_t esp_timer_get_time(void)
{
    return tb_fake_time_us();
}

#endif /* TB_TEST_FAKES_ESP_TIMER_H */
