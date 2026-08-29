#include "ui_nav.h"

#include <stddef.h>

#include "ui_session.h"

static ui_screen_show_fn s_show[UI_SCREEN_COUNT];
static ui_screen_id_t s_current = UI_SCREEN_HOME;
static ui_age_band_t s_pending_age = UI_AGE_BAND_6_17;
static ui_gender_t s_pending_gender = UI_GENDER_M;
/*
 * "Tidak ada" starts highlighted. The safe default for a HIGHLIGHT is the common
 * answer, not the cautious one: it is committed only by a deliberate Select, and
 * pre-selecting "Ada" would mean a rushed double-press marks a walking patient
 * RED. The unanswered case is carried by ui_session_has_airway(), not by this.
 */
static bool s_pending_airway;

static int screen_is_valid(ui_screen_id_t id)
{
    return id >= UI_SCREEN_HOME && id < UI_SCREEN_COUNT;
}

static void reset_pending_selections(void)
{
    s_pending_age = UI_AGE_BAND_6_17;
    s_pending_gender = UI_GENDER_M;
    s_pending_airway = false;
}

void ui_nav_register(ui_screen_id_t id, ui_screen_show_fn show)
{
    if (screen_is_valid(id)) {
        s_show[id] = show;
    }
}

void ui_nav_go(ui_screen_id_t id)
{
    if (!screen_is_valid(id)) {
        return;
    }

    if (id == UI_SCREEN_HOME || id == UI_SCREEN_SCANNING) {
        ui_session_reset();
        reset_pending_selections();
    }

    s_current = id;
    if (s_show[id] != NULL) {
        s_show[id]();
    }
}

ui_screen_id_t ui_nav_current(void)
{
    return s_current;
}

void ui_nav_set_pending_age(ui_age_band_t age)
{
    if (age >= UI_AGE_BAND_6_17 && age <= UI_AGE_BAND_OVER_60) {
        s_pending_age = age;
    }
}

ui_age_band_t ui_nav_pending_age(void)
{
    return s_pending_age;
}

void ui_nav_set_pending_gender(ui_gender_t gender)
{
    if (gender == UI_GENDER_M || gender == UI_GENDER_F) {
        s_pending_gender = gender;
    }
}

ui_gender_t ui_nav_pending_gender(void)
{
    return s_pending_gender;
}

void ui_nav_set_pending_airway(bool problem)
{
    s_pending_airway = problem;
}

bool ui_nav_pending_airway(void)
{
    return s_pending_airway;
}

void ui_nav_move_pending_age(int dir)
{
    int next = (int)s_pending_age + dir;

    if (next < (int)UI_AGE_BAND_6_17) {
        next = (int)UI_AGE_BAND_6_17;
    } else if (next > (int)UI_AGE_BAND_OVER_60) {
        next = (int)UI_AGE_BAND_OVER_60;
    }
    s_pending_age = (ui_age_band_t)next;
}

void ui_nav_move_pending_gender(int dir)
{
    if (dir < 0) {
        s_pending_gender = UI_GENDER_M;
    } else if (dir > 0) {
        s_pending_gender = UI_GENDER_F;
    }
}

/* Two rows, so any downward move lands on "Ada" and any upward on "Tidak ada" --
 * same shape as gender, and it means holding a key cannot walk past the end. */
void ui_nav_move_pending_airway(int dir)
{
    if (dir < 0) {
        s_pending_airway = false;
    } else if (dir > 0) {
        s_pending_airway = true;
    }
}

void ui_nav_on_rfid_ready(const rfid_t *rfid)
{
    if (s_current != UI_SCREEN_SCANNING || rfid == NULL || !rfid->present) {
        return;
    }

    ui_session_new_scan(rfid);
    s_current = UI_SCREEN_BERHASIL;
    if (s_show[UI_SCREEN_BERHASIL] != NULL) {
        s_show[UI_SCREEN_BERHASIL]();
    }
}

void ui_nav_on_measure_done(void)
{
    if (s_current == UI_SCREEN_MENGUKUR) {
        ui_nav_go(UI_SCREEN_RESULT);
    }
}
