#include "ui_action.h"

#include <stdbool.h>
#include <stddef.h>

#include "ui_nav.h"
#include "ui_session.h"

/*
 * LVGL v9 key codes (stable in lv_indev.h) — mirrored so this file stays
 * LVGL-free. ui_input maps btn0→PREV, btn1→NEXT, btn2→ENTER, btn3→ESC.
 */
enum {
    k_lv_key_prev  = 11, /* LV_KEY_PREV  */
    k_lv_key_next  = 9,  /* LV_KEY_NEXT  */
    k_lv_key_enter = 10, /* LV_KEY_ENTER */
    k_lv_key_esc   = 27  /* LV_KEY_ESC   */
};

static void action_noop(void)
{
}

static bool s_power_requested;
static bool s_beep_requested;
static bool s_menu_requested;

/* Only commit-type presses beep. In a disaster zone a click on every Up/Down
 * is noise the operator would learn to ignore. */
static void beep(void)
{
    s_beep_requested = true;
}

bool ui_action_take_beep_request(void)
{
    bool req = s_beep_requested;

    s_beep_requested = false;
    return req;
}

static void action_power(uint8_t btn_id)
{
    /* Raise a request only. ui_bindings owns the confirm dialog so this file
     * stays LVGL-free, and so a brushed button cannot kill a running measure. */
    (void)btn_id;
    s_power_requested = true;
}

bool ui_action_take_power_request(void)
{
    bool req = s_power_requested;

    s_power_requested = false;
    return req;
}

static void action_menu(uint8_t btn_id)
{
    /* Menu (3) had no meaning in the Figma flow; it now opens the settings
     * dialog ui_bindings owns. Request-only, same as power, so this file stays
     * LVGL-free -- and so the dialog can refuse to stack on top of itself. */
    (void)btn_id;
    s_menu_requested = true;
}

bool ui_action_take_menu_request(void)
{
    bool req = s_menu_requested;

    s_menu_requested = false;
    return req;
}

static void home_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 1: /* Scan */
        beep();
        ui_nav_go(UI_SCREEN_SCANNING);
        break;
    case 2: /* Power */
        action_power(btn_id);
        break;
    case 3: /* Menu */
        action_menu(btn_id);
        break;
    default: /* 0 empty */
        action_noop();
        break;
    }
}

static void scanning_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 0: /* Abort → Home */
        ui_nav_go(UI_SCREEN_HOME);
        break;
    case 2: /* Power */
        action_power(btn_id);
        break;
    case 3: /* Menu */
        action_menu(btn_id);
        break;
    default: /* 1 empty */
        action_noop();
        break;
    }
}

static void berhasil_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 0: /* Start → Age */
        beep();
        ui_nav_go(UI_SCREEN_AGE);
        break;
    case 1: /* Restart → Scanning (clear RFID/session) */
        beep();
        ui_nav_go(UI_SCREEN_SCANNING);
        break;
    case 2: /* Power */
        action_power(btn_id);
        break;
    case 3: /* Menu */
        action_menu(btn_id);
        break;
    default:
        action_noop();
        break;
    }
}

static void age_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 0: /* Up */
        ui_nav_move_pending_age(-1);
        break;
    case 1: /* Down */
        ui_nav_move_pending_age(1);
        break;
    case 2: /* Back → Berhasil */
        ui_nav_go(UI_SCREEN_BERHASIL);
        break;
    case 3: /* Select → Gender; commit pending age */
        beep();
        ui_session_set_age(ui_nav_pending_age());
        ui_nav_go(UI_SCREEN_GENDER);
        break;
    default:
        action_noop();
        break;
    }
}

static void gender_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 0: /* Up */
        ui_nav_move_pending_gender(-1);
        break;
    case 1: /* Down */
        ui_nav_move_pending_gender(1);
        break;
    case 2: /* Back → Age */
        ui_nav_go(UI_SCREEN_AGE);
        break;
    case 3: /* Select → Mengukur; commit pending gender */
        beep();
        ui_session_set_gender(ui_nav_pending_gender());
        ui_nav_go(UI_SCREEN_MENGUKUR);
        break;
    default:
        action_noop();
        break;
    }
}

static void mengukur_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 0: /* Abort → Home */
        ui_nav_go(UI_SCREEN_HOME);
        break;
    case 2: /* Power */
        action_power(btn_id);
        break;
    case 3: /* Menu */
        action_menu(btn_id);
        break;
    default: /* 1 empty */
        action_noop();
        break;
    }
}

static void result_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 0: /* Monitor */
        ui_nav_go(UI_SCREEN_MONITOR);
        break;
    case 1: /* Reset → Home + session clear (ui_nav_go HOME resets) */
        beep();
        ui_nav_go(UI_SCREEN_HOME);
        break;
    case 2: /* Power */
        action_power(btn_id);
        break;
    case 3: /* Menu */
        action_menu(btn_id);
        break;
    default:
        action_noop();
        break;
    }
}

static void monitor_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 0: /* Back → Result */
        ui_nav_go(UI_SCREEN_RESULT);
        break;
    case 1: /* Stop → Result */
        beep();
        ui_nav_go(UI_SCREEN_RESULT);
        break;
    case 2: /* Power */
        action_power(btn_id);
        break;
    case 3: /* Menu */
        action_menu(btn_id);
        break;
    default:
        action_noop();
        break;
    }
}

typedef void (*ui_screen_action_fn)(uint8_t btn_id);

static const ui_screen_action_fn s_tables[UI_SCREEN_COUNT] = {
    home_action,
    scanning_action,
    berhasil_action,
    age_action,
    gender_action,
    mengukur_action,
    result_action,
    monitor_action,
};

void ui_action(ui_screen_id_t screen, uint8_t btn_id)
{
    if (screen >= UI_SCREEN_COUNT || btn_id > 3U) {
        return;
    }
    if (s_tables[screen] != NULL) {
        s_tables[screen](btn_id);
    }
}

void ui_action_on_key(uint32_t lv_key)
{
    uint8_t btn_id;

    switch (lv_key) {
    case k_lv_key_prev:
        btn_id = 0;
        break;
    case k_lv_key_next:
        btn_id = 1;
        break;
    case k_lv_key_enter:
        btn_id = 2;
        break;
    case k_lv_key_esc:
        btn_id = 3;
        break;
    default:
        return;
    }

    ui_action(ui_nav_current(), btn_id);
}
