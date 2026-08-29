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
    "AIRWAY",
    "MENGUKUR",
    "RESULT",
    "MONITOR",
};

static void expect(ui_screen_id_t expected)
{
    assert(ui_nav_current() == expected);
    printf("%s\n", screen_names[expected]);
}

/*
 * The airway answer is the only manual input that can force RED on its own, so
 * what is pinned here is that it takes a DELIBERATE act to say yes: moving the
 * highlight is not answering, and leaving the screen backwards is not answering
 * either.
 */
static void test_airway_needs_a_deliberate_select(void)
{
    ui_nav_go(UI_SCREEN_HOME); /* resets the session and the pending rows */
    ui_nav_go(UI_SCREEN_AIRWAY);
    assert(!ui_nav_pending_airway()); /* "Tidak ada" starts highlighted */
    assert(!ui_session_has_airway());

    /* Two rows, so movement saturates rather than walking off either end. */
    ui_action(UI_SCREEN_AIRWAY, 0U); /* Up, already at the top */
    assert(!ui_nav_pending_airway());
    ui_action(UI_SCREEN_AIRWAY, 1U); /* Down -> "Ada / tersumbat" */
    assert(ui_nav_pending_airway());
    ui_action(UI_SCREEN_AIRWAY, 1U); /* Down again, already at the bottom */
    assert(ui_nav_pending_airway());

    /* Highlighting "Ada" is not answering: nothing is committed yet. */
    assert(!ui_session_has_airway());

    /* Back does not commit either. This is the one that matters -- a highlight
     * left on "Ada" plus a Back must not file the patient as obstructed. */
    ui_action(UI_SCREEN_AIRWAY, 2U);
    expect(UI_SCREEN_GENDER);
    assert(!ui_session_has_airway());

    /* Select does, and it carries the highlighted row through. */
    ui_action(UI_SCREEN_GENDER, 3U);
    expect(UI_SCREEN_AIRWAY);
    ui_action(UI_SCREEN_AIRWAY, 1U);
    ui_action(UI_SCREEN_AIRWAY, 3U);
    expect(UI_SCREEN_MENGUKUR);
    assert(ui_session_has_airway());
    assert(ui_session_get_airway());

    /* And a new patient starts unanswered again, highlight included: the
     * previous patient's obstructed airway must not follow the next one. */
    ui_nav_go(UI_SCREEN_HOME);
    assert(!ui_session_has_airway());
    assert(!ui_nav_pending_airway());
}

int main(void)
{
    rfid_t rfid = { "3021", true };

    test_airway_needs_a_deliberate_select();

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
    /* Third manual input. Select commits the airway answer and only then does
     * measuring start -- the model treats a set airway flag as RED on its own,
     * so it has to be asked before the reading, not after. */
    expect(UI_SCREEN_AIRWAY);
    assert(!ui_session_has_airway());
    ui_action(UI_SCREEN_AIRWAY, 3U);
    expect(UI_SCREEN_MENGUKUR);
    assert(ui_session_has_airway());
    assert(!ui_session_get_airway()); /* "Tidak ada" is the pre-highlighted row */

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
