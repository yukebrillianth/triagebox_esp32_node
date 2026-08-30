#ifndef UI_LOGIC_UI_NAV_H
#define UI_LOGIC_UI_NAV_H

#include "ui_types.h"

typedef enum {
    UI_SCREEN_HOME = 0,
    UI_SCREEN_SCANNING,
    UI_SCREEN_BERHASIL,
    UI_SCREEN_AGE,
    UI_SCREEN_GENDER,
    /* Third manual input, in flow order after Gender. Anything indexed by this
     * enum positionally -- ui_action's s_tables, ui_bindings' k_screen_ids --
     * has to be updated in step, which is deliberate: a silently shifted table
     * would give one screen another screen's buttons. */
    UI_SCREEN_AIRWAY,
    UI_SCREEN_MENGUKUR,
    UI_SCREEN_RESULT,
    UI_SCREEN_MONITOR,
    /* Range test / diagnostics, reached from the Menu and not part of the patient
     * flow. Appended AFTER Monitor on purpose: the tables indexed by this enum
     * would otherwise all shift by one, which is the trap the comment above
     * describes. */
    UI_SCREEN_TEST,
    UI_SCREEN_COUNT
} ui_screen_id_t;

typedef void (*ui_screen_show_fn)(void);

void ui_nav_register(ui_screen_id_t id, ui_screen_show_fn show);
void ui_nav_go(ui_screen_id_t id);
ui_screen_id_t ui_nav_current(void);

/*
 * Return from a screen that is not part of the linear flow (Test). Returns to
 * whichever screen was showing when Test was opened, or Home as a fallback.
 */
void ui_nav_back_from_test(void);

/* List screens update these as focus moves; Select commits them to session. */
void ui_nav_set_pending_age(ui_age_band_t age);
ui_age_band_t ui_nav_pending_age(void);
void ui_nav_set_pending_gender(ui_gender_t gender);
ui_gender_t ui_nav_pending_gender(void);
/* Airway is a yes/no list, so "pending" is just which row is highlighted. */
void ui_nav_set_pending_airway(bool problem);
bool ui_nav_pending_airway(void);

/* Move list focus by dir (-1 up, +1 down), clamped to the band/gender range. */
void ui_nav_move_pending_age(int dir);
void ui_nav_move_pending_gender(int dir);
void ui_nav_move_pending_airway(int dir);

/* Non-button transitions delivered by the mock/hardware integration layer. */
void ui_nav_on_rfid_ready(const rfid_t *rfid);
void ui_nav_on_measure_done(void);

#endif /* UI_LOGIC_UI_NAV_H */
