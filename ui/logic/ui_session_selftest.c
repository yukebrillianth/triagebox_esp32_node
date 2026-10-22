/* ui_session: the time the displayed blood pressure last changed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui_session.h"

static time_t s_now;
static time_t fake_clock(void) { return s_now; }

static vitals_t with_bp(uint16_t sys, uint16_t dia, uint8_t mask)
{
    vitals_t v;

    memset(&v, 0, sizeof(v));
    v.bp_sys = sys;
    v.bp_dia = dia;
    v.valid_mask = mask;
    return v;
}

int main(void)
{
    ui_session_set_clock(fake_clock);
    ui_session_reset();
    assert(ui_session_bp_time() == 0);

    /* HR alone is not a pressure: no time yet. */
    vitals_t v = with_bp(0, 0, UI_VITAL_HR);
    s_now = 1000;
    ui_session_set_vitals(&v);
    assert(ui_session_bp_time() == 0);

    /* First pressure stamps the time. */
    v = with_bp(118, 76, UI_VITAL_BP | UI_VITAL_HR);
    s_now = 2000;
    ui_session_set_vitals(&v);
    assert(ui_session_bp_time() == 2000);

    /* Same numbers later, even through a flickering valid bit: still 2000. */
    s_now = 3000;
    ui_session_set_vitals(&v);
    v = with_bp(0, 0, UI_VITAL_HR);
    s_now = 3500;
    ui_session_set_vitals(&v);
    v = with_bp(118, 76, UI_VITAL_BP);
    s_now = 4000;
    ui_session_set_vitals(&v);
    assert(ui_session_bp_time() == 2000);

    /* A different pair is a new measurement. */
    v = with_bp(131, 86, UI_VITAL_BP);
    s_now = 5000;
    ui_session_set_vitals(&v);
    assert(ui_session_bp_time() == 5000);

    /* A new patient starts from nothing. */
    ui_session_reset();
    assert(ui_session_bp_time() == 0);

    ui_session_set_clock(NULL);
    printf("ui_session_selftest: OK\n");
    return 0;
}
