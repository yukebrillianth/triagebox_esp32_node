#include "ui_action.h"

#include <stdio.h>

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

static void action_power_menu_noop(uint8_t btn_id)
{
    /* Power (2) / Menu (3) — undefined in Figma flow. */
    (void)btn_id;
    printf("ui_action: power/menu no-op (btn=%u screen=%d)\n",
           (unsigned)btn_id, (int)ui_nav_current());
}

static void home_action(uint8_t btn_id)
{
    switch (btn_id) {
    case 1: /* Scan */
        ui_nav_go(UI_SCREEN_SCANNING);
        break;
    case 2: /* Power */
    case 3: /* Menu */
        action_power_menu_noop(btn_id);
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
    case 2:
    case 3:
        action_power_menu_noop(btn_id);
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
        ui_nav_go(UI_SCREEN_AGE);
        break;
    case 1: /* Restart → Scanning (clear RFID/session) */
        ui_nav_go(UI_SCREEN_SCANNING);
        break;
    case 2:
    case 3:
        action_power_menu_noop(btn_id);
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
    case 2:
    case 3:
        action_power_menu_noop(btn_id);
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
        ui_nav_go(UI_SCREEN_HOME);
        break;
    case 2:
    case 3:
        action_power_menu_noop(btn_id);
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
        ui_nav_go(UI_SCREEN_RESULT);
        break;
    case 2:
    case 3:
        action_power_menu_noop(btn_id);
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
