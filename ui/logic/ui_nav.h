#ifndef UI_LOGIC_UI_NAV_H
#define UI_LOGIC_UI_NAV_H

#include "ui_types.h"

typedef enum {
    UI_SCREEN_HOME = 0,
    UI_SCREEN_SCANNING,
    UI_SCREEN_BERHASIL,
    UI_SCREEN_AGE,
    UI_SCREEN_GENDER,
    UI_SCREEN_MENGUKUR,
    UI_SCREEN_RESULT,
    UI_SCREEN_MONITOR,
    UI_SCREEN_COUNT
} ui_screen_id_t;

typedef void (*ui_screen_show_fn)(void);

void ui_nav_register(ui_screen_id_t id, ui_screen_show_fn show);
void ui_nav_go(ui_screen_id_t id);
ui_screen_id_t ui_nav_current(void);

/* List screens update these as focus moves; Select commits them to session. */
void ui_nav_set_pending_age(ui_age_band_t age);
ui_age_band_t ui_nav_pending_age(void);
void ui_nav_set_pending_gender(ui_gender_t gender);
ui_gender_t ui_nav_pending_gender(void);

/* Non-button transitions delivered by the mock/hardware integration layer. */
void ui_nav_on_rfid_ready(const rfid_t *rfid);
void ui_nav_on_measure_done(void);

#endif /* UI_LOGIC_UI_NAV_H */
