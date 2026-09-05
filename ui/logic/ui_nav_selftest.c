#include "ui_action.h"
#include "ui_mock.h"
#include "ui_nav.h"
#include "ui_session.h"

#include <assert.h>
#include <stdio.h>

/*
 * ui_nav.c's only outward call, stubbed here as a counter rather than linked
 * from ui_mock.c. That call is the ABORT that tells the sensor board its patient
 * is gone -- on hardware it is the difference between a departed patient
 * disappearing from the dashboard and being re-reported every 15 s with blank
 * measurements -- so WHEN it happens is worth pinning, and the sim's own
 * implementation has nothing observable to pin against.
 */
static unsigned s_end_session_calls;

void ui_mock_end_session(void)
{
    ++s_end_session_calls;
}

static const char *const screen_names[UI_SCREEN_COUNT] = {
    "HOME",
    "SCANNING",
    "BERHASIL",
    "AGE",
    "GENDER",
    "AIRWAY",
    "RR",
    "MENGUKUR",
    "RESULT",
    "MONITOR",
    "TEST",
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
    expect(UI_SCREEN_RR);
    assert(ui_session_has_airway());
    assert(ui_session_get_airway());

    /* And a new patient starts unanswered again, highlight included: the
     * previous patient's obstructed airway must not follow the next one. */
    ui_nav_go(UI_SCREEN_HOME);
    assert(!ui_session_has_airway());
    assert(!ui_nav_pending_airway());
}

/*
 * The Test screen hangs off the Menu, so it is reachable from anywhere -- which
 * means Back has to land where the operator was, not on a fixed screen. What is
 * pinned here is that the return address follows the screen it was opened from,
 * and that visiting it does not disturb the patient in the session.
 */
static void test_screen_returns_where_it_came_from(void)
{
    rfid_t rfid = { "3021", true };

    ui_nav_go(UI_SCREEN_HOME);
    ui_nav_go(UI_SCREEN_TEST);
    ui_action(UI_SCREEN_TEST, 0U);
    expect(UI_SCREEN_HOME);

    /* From mid-flow, with a patient in the session. */
    ui_action(UI_SCREEN_HOME, 1U);
    ui_nav_on_rfid_ready(&rfid);
    expect(UI_SCREEN_BERHASIL);
    ui_nav_go(UI_SCREEN_TEST);
    assert(ui_session_has_rfid()); /* Test is not part of the flow: no reset */
    ui_action(UI_SCREEN_TEST, 0U);
    expect(UI_SCREEN_BERHASIL);
    assert(ui_session_has_rfid());

    /* Opening it twice in a row must not make Back a no-op that traps the
     * operator on the diagnostics screen. */
    ui_nav_go(UI_SCREEN_TEST);
    ui_nav_go(UI_SCREEN_TEST);
    ui_action(UI_SCREEN_TEST, 0U);
    expect(UI_SCREEN_BERHASIL);
}

/*
 * Home is the one exit that ends the patient's session on the sensor board.
 *
 * Result and Monitor are the only screens after a verdict, and the station
 * publishes a vital for as long as a verdict stands -- so an operator who walks
 * away without pressing Reset used to leave the departed patient on the
 * dashboard, re-reported every 15 s with blank measurements that bury the next
 * patient's real ones.
 *
 * What is pinned is the exclusions as much as the rule. Result<->Monitor must NOT
 * end it: Back, Stop and the re-triage jump all move inside the pair while the
 * patient is still wired up. Test must not either: it is the range-test screen
 * opened from the Menu, and Back returns to exactly where it came from. And Home
 * must do it EXACTLY ONCE -- two ABORTs are harmless on the wire, but would mean
 * a second branch fired, which is the kind of double-send that hides an ordering
 * bug. (There used to be that second branch, for "leaving the pair for anything
 * that is not Home or Test". It was dead: from Result the nav targets are
 * Monitor/Home/Test and from Monitor only Result/Test, so nothing could reach
 * it.)
 */
static void test_home_is_the_only_end_of_session(void)
{
    unsigned before;

    ui_nav_go(UI_SCREEN_HOME);

    /* Inside the pair: silent, both directions. */
    ui_nav_go(UI_SCREEN_RESULT);
    before = s_end_session_calls;
    ui_nav_go(UI_SCREEN_MONITOR);
    ui_nav_go(UI_SCREEN_RESULT);
    assert(s_end_session_calls == before);

    /* Out of the pair to Test: still silent, and Back comes straight back. */
    ui_nav_go(UI_SCREEN_TEST);
    assert(s_end_session_calls == before);
    ui_nav_back_from_test();
    expect(UI_SCREEN_RESULT);
    assert(s_end_session_calls == before);

    /* Home from Result, then Home from Monitor: once each, never twice. */
    ui_nav_go(UI_SCREEN_HOME);
    assert(s_end_session_calls == before + 1U);
    ui_nav_go(UI_SCREEN_MONITOR);
    before = s_end_session_calls;
    ui_nav_go(UI_SCREEN_HOME);
    assert(s_end_session_calls == before + 1U);

    /* And a screen that is not part of the pair does not trigger it at all. */
    before = s_end_session_calls;
    ui_nav_go(UI_SCREEN_AGE);
    ui_nav_go(UI_SCREEN_GENDER);
    assert(s_end_session_calls == before);
    printf("end_session: on Home only, once\n");
}

int main(void)
{
    rfid_t rfid = { "3021", true };

    test_airway_needs_a_deliberate_select();
    test_screen_returns_where_it_came_from();
    test_home_is_the_only_end_of_session();

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
    /* Fourth manual input, and the one that decides whether the model scores at
     * all: tb_classify() refuses on respiratory_rate <= 0. */
    expect(UI_SCREEN_RR);
    assert(ui_session_has_airway());
    assert(!ui_session_get_airway()); /* "Tidak ada" is the pre-highlighted row */
    assert(!ui_session_has_rr());
    ui_action(UI_SCREEN_RR, 3U);
    expect(UI_SCREEN_MENGUKUR);
    assert(ui_session_has_rr());
    assert(ui_session_get_rr() == UI_RR_BAND_12_20); /* pre-highlighted row */

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
