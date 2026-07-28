#include "ui_action.h"
#include "ui_nav.h"
#include "ui_session.h"

#include <assert.h>
#include <stdio.h>

static const char *const screen_names[UI_SCREEN_COUNT] = {
    "HOME",
    "SCANNING",
    "BERHASIL",
    "AGE",
    "GENDER",
    "MENGUKUR",
    "RESULT",
    "MONITOR",
};

static void expect(ui_screen_id_t expected)
{
    assert(ui_nav_current() == expected);
    printf("%s\n", screen_names[expected]);
}

int main(void)
{
    rfid_t rfid = { "3021", true };

    ui_nav_go(UI_SCREEN_HOME);
    expect(UI_SCREEN_HOME);

    ui_action(UI_SCREEN_HOME, 1U);
    expect(UI_SCREEN_SCANNING);

    ui_nav_on_rfid_ready(&rfid);
    expect(UI_SCREEN_BERHASIL);
    assert(ui_session_has_rfid());

    ui_action(UI_SCREEN_BERHASIL, 0U);
    expect(UI_SCREEN_AGE);
    ui_action(UI_SCREEN_AGE, 3U);
    expect(UI_SCREEN_GENDER);
    ui_action(UI_SCREEN_GENDER, 3U);
    expect(UI_SCREEN_MENGUKUR);

    ui_nav_on_measure_done();
    expect(UI_SCREEN_RESULT);
    ui_action(UI_SCREEN_RESULT, 0U);
    expect(UI_SCREEN_MONITOR);
    ui_action(UI_SCREEN_MONITOR, 0U);
    expect(UI_SCREEN_RESULT);
    ui_action(UI_SCREEN_RESULT, 1U);
    expect(UI_SCREEN_HOME);
    assert(!ui_session_has_rfid());

    return 0;
}
