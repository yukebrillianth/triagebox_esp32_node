#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "ui_bindings.h"
#include "../ui.h"
#include "ui_action.h"
#include "ui_airway.h"
#include "ui_board.h"
#include "ui_demo.h"
#include "ui_mock.h"
#include "ui_nav.h"
#include "ui_session.h"
#include "ui_status.h"

static const ui_screen_id_t k_screen_ids[UI_SCREEN_COUNT] = {
    UI_SCREEN_HOME,
    UI_SCREEN_SCANNING,
    UI_SCREEN_BERHASIL,
    UI_SCREEN_AGE,
    UI_SCREEN_GENDER,
    UI_SCREEN_AIRWAY,
    UI_SCREEN_MENGUKUR,
    UI_SCREEN_RESULT,
    UI_SCREEN_MONITOR,
};

static lv_obj_t *screen_root(ui_screen_id_t id)
{
    switch (id) {
    case UI_SCREEN_HOME:     return home;
    case UI_SCREEN_SCANNING: return scanning;
    case UI_SCREEN_BERHASIL: return berhasil;
    case UI_SCREEN_AGE:      return age;
    case UI_SCREEN_GENDER:   return gender;
    /* Not a generated global like the other eight: this screen is built in C,
     * so ui_airway_screen() both creates and caches it. Safe to call on every
     * status-bar tick -- the second call onward just returns the cache. */
    case UI_SCREEN_AIRWAY:   return ui_airway_screen();
    case UI_SCREEN_MENGUKUR: return mengukur;
    case UI_SCREEN_RESULT:   return result;
    case UI_SCREEN_MONITOR:  return monitor;
    default:                 return NULL;
    }
}

static void cell_clicked_cb(lv_event_t *e)
{
    uintptr_t packed = (uintptr_t)lv_event_get_user_data(e);
    ui_screen_id_t screen = (ui_screen_id_t)(packed >> 8);
    uint8_t btn_id = (uint8_t)(packed & 0xFFU);

    ui_action(screen, btn_id);
}

static void bind_button_bar(ui_screen_id_t id)
{
    lv_obj_t *root = screen_root(id);
    lv_obj_t *bar;
    uint8_t i;
    static const char *cell_names[4] = {"cell0", "cell1", "cell2", "cell3"};

    if (root == NULL) {
        return;
    }
    bar = lv_obj_find_by_name(root, "button_bar_#");
    if (bar == NULL) {
        bar = root;
    }

    for (i = 0; i < 4U; i++) {
        lv_obj_t *cell = lv_obj_find_by_name(bar, cell_names[i]);
        uintptr_t packed;

        if (cell == NULL) {
            continue;
        }
        packed = ((uintptr_t)id << 8) | (uintptr_t)i;
        lv_obj_add_event_cb(cell, cell_clicked_cb, LV_EVENT_CLICKED, (void *)packed);
    }
}

static void age_option_cb(lv_event_t *e)
{
    ui_age_band_t band = (ui_age_band_t)(uintptr_t)lv_event_get_user_data(e);

    ui_nav_set_pending_age(band);
    ui_bindings_sync_selection();
}

static void gender_option_cb(lv_event_t *e)
{
    ui_gender_t g = (ui_gender_t)(uintptr_t)lv_event_get_user_data(e);

    ui_nav_set_pending_gender(g);
    ui_bindings_sync_selection();
}

/* Touch only moves the highlight, exactly like Age and Gender. Committing stays
 * with Select, so a stray tap on "Ada / tersumbat" cannot mark a patient RED. */
static void airway_option_cb(lv_event_t *e)
{
    bool problem = (bool)(uintptr_t)lv_event_get_user_data(e);

    ui_nav_set_pending_airway(problem);
    ui_bindings_sync_selection();
}

static void bind_option(lv_obj_t *root, const char *name, lv_event_cb_t cb, uintptr_t value)
{
    lv_obj_t *opt;

    if (root == NULL) {
        return;
    }
    opt = lv_obj_find_by_name(root, name);
    if (opt == NULL) {
        return;
    }
    lv_obj_add_event_cb(opt, cb, LV_EVENT_CLICKED, (void *)value);
}

/* ------------------------------------------------------ Home status dots -- */

static lv_color_t status_color(ui_status_state_t state)
{
    switch (state) {
    case UI_STATUS_OK:   return COLOR_STATUS_OK;
    case UI_STATUS_WARN: return COLOR_STATUS_WARN;
    default:             return COLOR_STATUS_ERROR;
    }
}

static void set_dot(const char *dot_name, const char *text_name,
                    const char *prefix, ui_status_state_t state)
{
    lv_obj_t *dot;
    lv_obj_t *label;
    char buf[24];

    if (home == NULL) {
        return;
    }
    dot = lv_obj_find_by_name(home, dot_name);
    if (dot != NULL) {
        lv_obj_set_style_bg_color(dot, status_color(state), 0);
    }
    label = lv_obj_find_by_name(home, text_name);
    if (label != NULL) {
        ui_status_label(buf, sizeof(buf), prefix, state);
        lv_label_set_text(label, buf);
    }
}

void ui_bindings_sync_status_dots(void)
{
    link_status_t s;

    /* Only Home shows the dots; skip the lookups everywhere else. */
    if (ui_nav_current() != UI_SCREEN_HOME) {
        return;
    }

    ui_mock_get_link_status(&s);
    set_dot("stat_dot_sys", "stat_text_sys", "Sistem",
            ui_status_system(s.link_age_ms, s.link_never_seen));
    set_dot("stat_dot_sensor", "stat_text_sensor", "Sensor",
            ui_status_sensors(s.sensor_mask));
    set_dot("stat_dot_lora", "stat_text_lora", "LoRa",
            ui_status_lora(s.lora_ok, s.lora_reported));
}

/* ------------------------------------------------------ Status bar -------- */

/*
 * Replaces the authored "80%" / "Connected" / "--:--" literals that every
 * *_gen.c passes to status_bar_create(). The bar is on all eight screens, so
 * unlike the Home dots this must not early-out on the current screen -- it syncs
 * whichever one is showing.
 *
 * Rate-limited on purpose. The clock has minute resolution and the gauge moves
 * in 1% steps, so 50 ms would be pure I2C contention for no visible change --
 * and the PMIC shares a bus with the touch controller with no cross-component
 * lock, which is already a suspect for the random resets.
 */
#define STATUS_BAR_TICKS   20U  /* 20 x 50 ms = 1 s: enough for a HH:MM clock */
#define BATTERY_READ_TICKS 200U /* 10 s: a 1%/step gauge cannot move faster    */

static const void *battery_icon_src(ui_battery_icon_t icon)
{
    switch (icon) {
    case UI_BATTERY_ICON_CHARGING: return battery_charging;
    case UI_BATTERY_ICON_FULL:     return battery_full;
    case UI_BATTERY_ICON_MEDIUM:   return battery_medium;
    default:                       return battery_empty;
    }
}

static void sync_status_bar(void)
{
    static uint32_t s_ticks;
    static uint8_t s_percent = UI_BATTERY_UNKNOWN;
    static bool s_charging;
    lv_obj_t *root = screen_root(ui_nav_current());
    lv_obj_t *obj;
    link_status_t s;
    bool refresh = (s_ticks % STATUS_BAR_TICKS) == 0U;
    bool reread = (s_ticks % BATTERY_READ_TICKS) == 0U;
    char buf[8];

    s_ticks++;
    if (root == NULL || !refresh) {
        return;
    }

    if (reread) {
        uint8_t pct;
        bool chg;

        /* A failed read resets to UNKNOWN rather than keeping the last good
         * value: a frozen 80% while the pack drains is worse than "--%". */
        if (ui_board_battery(&pct, &chg)) {
            s_percent = pct;
            s_charging = chg;
        } else {
            s_percent = UI_BATTERY_UNKNOWN;
            s_charging = false;
        }
    }

    obj = lv_obj_find_by_name(root, "sb_battery_text");
    if (obj != NULL) {
        ui_status_battery_text(buf, sizeof(buf), s_percent);
        lv_label_set_text(obj, buf);
    }
    obj = lv_obj_find_by_name(root, "sb_battery");
    if (obj != NULL) {
        lv_image_set_src(obj, battery_icon_src(
                                  ui_status_battery_icon(s_percent, s_charging)));
    }

    ui_mock_get_link_status(&s);
    obj = lv_obj_find_by_name(root, "sb_link_text");
    if (obj != NULL) {
        char link[UI_LINK_TEXT_MIN];
        ui_status_state_t state;

        ui_status_link_text(link, sizeof(link), s.lora_ok, s.lora_reported,
                            s.lora_rssi_dbm, s.lora_rssi_valid);
        lv_label_set_text(obj, link);
        /*
         * When there is a measured dBm the colour tracks the LINK MARGIN, not
         * merely "the radio initialised" -- that is the whole point of showing
         * the number while walking the box away from the station: it goes amber
         * before the link dies, which a boolean never does. Falls back to the
         * radio's own state before the first poll is heard.
         *
         * The signal glyph next to it is unnamed in the generated component, so
         * colour the text instead -- same information, no XML regeneration.
         */
        state = (s.lora_reported && s.lora_ok && s.lora_rssi_valid)
                    ? ui_status_rssi_state(s.lora_rssi_dbm)
                    : ui_status_lora(s.lora_ok, s.lora_reported);
        lv_obj_set_style_text_color(obj, status_color(state), 0);
    }

    obj = lv_obj_find_by_name(root, "sb_clock");
    if (obj != NULL) {
        char clock[6];

        ui_status_format_clock(clock, sizeof(clock));
        lv_label_set_text(obj, clock);
    }
}

/* ------------------------------------------------------ Dialogs ----------- */

/*
 * Hand-built instead of lv_msgbox: the stock msgbox brings LVGL's own theme
 * (light grey, its own radius and fonts) and looks nothing like the dark Figma
 * screens. Tokens below come from ui/globals.xml via ui_gen.h — no raw hex.
 *
 * One at a time, deliberately: s_dialog is both the handle and the "something is
 * already asking" guard, so Menu cannot open on top of the power confirm (or the
 * other way round) and leave a scrim nobody can dismiss.
 */
static lv_obj_t *s_dialog;
/* Which dialog is up, so Menu can close its own instead of being swallowed --
 * the dialogs are touch-only, and without this someone driving the box from the
 * four physical buttons could open the menu and have no way to dismiss it. */
static bool s_dialog_is_menu;

static void dialog_close(void)
{
    if (s_dialog != NULL) {
        lv_obj_delete(s_dialog);
        s_dialog = NULL;
        s_dialog_is_menu = false;
    }
}

/* Scrim + card. Returns the card, laid out as a column: title, body, footer. */
static lv_obj_t *dialog_card(int32_t w, int32_t h)
{
    lv_obj_t *card;

    /* Full-screen scrim: dims the screen behind and swallows stray touches so
     * the ButtonBar underneath cannot be pressed while the dialog is up. */
    s_dialog = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_dialog);
    lv_obj_set_size(s_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_dialog, COLOR_DARK_BG, 0);
    lv_obj_set_style_bg_opa(s_dialog, LV_OPA_70, 0);
    lv_obj_remove_flag(s_dialog, LV_OBJ_FLAG_SCROLLABLE);

    card = lv_obj_create(s_dialog);
    lv_obj_set_size(card, w, h);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, COLOR_DARK_PANEL, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    /* Square with a 1px hard border, same treatment as the bottom ButtonBar. */
    lv_obj_set_style_radius(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COLOR_DARK_PANEL, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(card, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, SPACE_XL, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    return card;
}

static void dialog_title(lv_obj_t *card, const char *text)
{
    lv_obj_t *title = lv_label_create(card);

    lv_label_set_text(title, text);
    lv_obj_set_style_text_color(title, COLOR_DARK_TEXT, 0);
    if (font_inter_bold_24 != NULL) {
        lv_obj_set_style_text_font(title, font_inter_bold_24, 0);
    }
}

/* warn = amber instead of grey, for a body that says something will be lost. */
static void dialog_body(lv_obj_t *card, const char *text, bool warn)
{
    lv_obj_t *body = lv_label_create(card);

    lv_label_set_text(body, text);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(body, warn ? COLOR_STATUS_WARN
                                           : COLOR_TEXT_ON_CARD, 0);
    if (font_inter_regular_16 != NULL) {
        lv_obj_set_style_text_font(body, font_inter_regular_16, 0);
    }
}

static lv_obj_t *dialog_row(lv_obj_t *card)
{
    lv_obj_t *row = lv_obj_create(card);

    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, SPACE_LG, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

/* One dialog button: 130x48, square, ButtonBar-style gradient + 1px border. */
static lv_obj_t *dialog_button(lv_obj_t *parent, const char *text, bool danger,
                               lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *label;

    lv_obj_set_size(btn, 130, 48);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_FULL, 0);
    if (danger) {
        lv_obj_set_style_bg_color(btn, COLOR_DANGER, 0);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_border_color(btn, COLOR_DANGER, 0);
    } else {
        /* Same vertical gradient as style_buttonbar_cell_grad in globals.xml. */
        lv_obj_set_style_bg_color(btn, COLOR_BUTTONBAR_TOP, 0);
        lv_obj_set_style_bg_grad_color(btn, COLOR_BUTTONBAR_BOTTOM, 0);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(btn, COLOR_DARK_PANEL, 0);
    }

    label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, COLOR_DARK_TEXT, 0);
    if (font_inter_semi_bold_16 != NULL) {
        lv_obj_set_style_text_font(label, font_inter_semi_bold_16, 0);
    }
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

/* ------------------------------------------------------ Power confirm ----- */

static void power_confirm_cb(lv_event_t *e)
{
    bool confirmed = (bool)(uintptr_t)lv_event_get_user_data(e);

    dialog_close();
    if (confirmed) {
        ui_mock_power_off();
    }
}

static void poll_power_request(void)
{
    lv_obj_t *card;
    lv_obj_t *row;
    bool measuring;

    if (!ui_action_take_power_request()) {
        return;
    }
    if (s_dialog != NULL) {
        return; /* already asking */
    }
    measuring = (ui_nav_current() == UI_SCREEN_MENGUKUR);

    card = dialog_card(400, 240);
    dialog_title(card, "Matikan alat?");
    /* Amber, not plain grey, when confirming actually destroys something. */
    dialog_body(card, measuring
                ? "Pengukuran sedang berjalan.\nData pasien akan hilang."
                : "Alat akan dimatikan.", measuring);
    row = dialog_row(card);

    /* Cancel first so the leftmost, easiest target is the safe one. */
    dialog_button(row, "Batal", false, power_confirm_cb, (void *)(uintptr_t)0);
    dialog_button(row, "Matikan", true, power_confirm_cb, (void *)(uintptr_t)1);
}

/* ------------------------------------------------------ Menu -------------- */

/*
 * Menu (btn 3), which the Figma flow left undefined.
 *
 * A settings LIST, not a pair of buttons. The first version was two buttons
 * ("Tutup" / "Demo: Aktifkan") and that shape does not survive a second entry:
 * the light/dark toggle coming next would make a row of three buttons where two
 * of them are settings and one is navigation, and the button label would have to
 * carry the current state ("Demo: Matikan" means demo is ON, which is backwards
 * from how a label usually reads).
 *
 * So: one row per setting, each with its own switch showing state directly, and
 * a single "Tutup" in the footer. Toggling does NOT close the dialog -- with more
 * than one setting, closing after each flip would mean reopening the menu to
 * change the second one.
 *
 * The card grows with its content (LV_SIZE_CONTENT), so adding a row needs no
 * size arithmetic.
 */
static void menu_close_cb(lv_event_t *e)
{
    (void)e;
    dialog_close();
}

/* The row's own sublabel says what the setting currently means. Found by name
 * rather than cached in a static: the label dies with the dialog, and a stale
 * pointer to a deleted object is a crash rather than a wrong string. */
static void menu_set_sub(const char *name, const char *text)
{
    lv_obj_t *label = (s_dialog != NULL) ? lv_obj_find_by_name(s_dialog, name)
                                         : NULL;

    if (label != NULL) {
        lv_label_set_text(label, text);
    }
}

static void menu_demo_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);

    ui_demo_set(lv_obj_has_state(sw, LV_STATE_CHECKED));
    menu_set_sub("menu_demo_sub", ui_demo_enabled()
                 ? "Vital & hasil triase palsu"
                 : "Vital dari sensor");
}

/*
 * One settings row: title + sublabel on the left, a switch on the right.
 *
 * A row helper with one caller is deliberate rather than premature. The
 * light/dark toggle is the next entry and the stated reason this screen was
 * redesigned, so the second call is what the shape is for. What was NOT done is
 * putting a dead "Tema" row on screen now: a switch that does nothing teaches
 * the operator the menu is broken, and the theme work is explicitly later.
 *
 * on_color is per row because the switches do not all mean the same kind of
 * thing -- see the demo row's amber below.
 */
static void menu_row(lv_obj_t *card, const char *sub_name, const char *title,
                     const char *sub, bool on, lv_color_t on_color,
                     lv_event_cb_t cb)
{
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_t *texts;
    lv_obj_t *label;
    lv_obj_t *sw;

    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(row, SPACE_MD, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    texts = lv_obj_create(row);
    lv_obj_remove_style_all(texts);
    lv_obj_set_size(texts, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(texts, SPACE_XS, 0);
    lv_obj_remove_flag(texts, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(texts);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, COLOR_DARK_TEXT, 0);
    if (font_inter_semi_bold_16 != NULL) {
        lv_obj_set_style_text_font(label, font_inter_semi_bold_16, 0);
    }

    label = lv_label_create(texts);
    lv_obj_set_name(label, sub_name);
    lv_label_set_text(label, sub);
    lv_obj_set_style_text_color(label, COLOR_TEXT_SECONDARY, 0);
    if (font_inter_regular_13 != NULL) {
        lv_obj_set_style_text_font(label, font_inter_regular_13, 0);
    }

    sw = lv_switch_create(row);
    /* 60x32 rather than LVGL's default 40x20: this is a gloved finger on a
     * 480x480 panel, not a mouse. */
    lv_obj_set_size(sw, 60, 32);
    lv_obj_set_style_bg_color(sw, COLOR_TRACK, 0);
    lv_obj_set_style_bg_color(sw, on_color, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, COLOR_DARK_TEXT, LV_PART_KNOB);
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    /* Handler added AFTER the initial state, or setting it would fire the
     * callback and toggle the very flag this row is reflecting. */
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void poll_menu_request(void)
{
    lv_obj_t *card;
    lv_obj_t *row;
    bool on;

    if (!ui_action_take_menu_request()) {
        return;
    }
    if (s_dialog != NULL) {
        /* Menu closes its own dialog, so the button that opened it can also
         * dismiss it. Without this the menu is unreachable-by-keypad: the rows
         * and "Tutup" are touch targets, so someone working the four physical
         * buttons would be stuck behind the scrim. A power confirm is left alone
         * -- it must be answered, not dismissed by a stray press. */
        if (s_dialog_is_menu) {
            dialog_close();
        }
        return;
    }
    on = ui_demo_enabled();

    card = dialog_card(420, LV_SIZE_CONTENT);
    s_dialog_is_menu = true;
    /* Content height, so the card is as tall as its rows: SPACE_BETWEEN has
     * nothing to distribute once the height is the content's. */
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, SPACE_LG, 0);

    dialog_title(card, "Menu");

    /*
     * Amber, not the accent green, when demo mode is on. Every other switch in
     * this menu will be an ordinary preference; this one makes the box report a
     * triage nobody measured, so it should not look like a preference. The
     * accent is also triage GREEN in this palette, which is the last colour a
     * "results are fake" control should borrow.
     */
    menu_row(card, "menu_demo_sub", "Mode demo",
             on ? "Vital & hasil triase palsu" : "Vital dari sensor",
             on, COLOR_STATUS_WARN, menu_demo_cb);

    row = dialog_row(card);
    dialog_button(row, "Tutup", false, menu_close_cb, NULL);
}

/* ------------------------------------------------------ Buzzer patterns --- */

/*
 * The buzzer is active (self-oscillating) and sits behind the I2C expander, so
 * it can only be switched on and off — there is no tone control, and toggling
 * at audio rate over I2C is not possible. Patterns are therefore pulse counts,
 * played by the 50 ms timer so nothing ever blocks the LVGL task with a delay.
 */
#define BEEP_TICK_MS 50U

static uint8_t s_beep_pulses;    /* pulses still to play */
static uint8_t s_beep_on_ticks;  /* ticks the buzzer stays on per pulse */
static uint8_t s_beep_phase;     /* ticks left in the current on/off phase */
static bool s_beep_is_on;

static void beep_start(uint8_t pulses, uint8_t on_ticks)
{
    s_beep_pulses = pulses;
    s_beep_on_ticks = on_ticks;
    s_beep_phase = 0;
    /* Next tick starts the first pulse; keeps all timing in one place. */
}

/* Drive the pattern one tick forward. Called from the 50 ms timer. */
static void beep_tick(void)
{
    if (s_beep_phase > 0) {
        s_beep_phase--;
        return;
    }
    if (s_beep_is_on) {
        ui_board_buzzer(false);
        s_beep_is_on = false;
        /* Gap between pulses; skip it after the last one. */
        s_beep_phase = (s_beep_pulses > 0) ? 1 : 0;
        return;
    }
    if (s_beep_pulses == 0) {
        return;
    }
    s_beep_pulses--;
    ui_board_buzzer(true);
    s_beep_is_on = true;
    s_beep_phase = s_beep_on_ticks;
}

/* Result patterns: the operator should hear the urgency without looking up. */
static void beep_for_priority(ui_priority_t p)
{
    switch (p) {
    case UI_PRIORITY_RED:    beep_start(3, 1); break; /* 3 short */
    case UI_PRIORITY_YELLOW: beep_start(2, 1); break;
    case UI_PRIORITY_GREEN:  beep_start(1, 1); break;
    default:                 beep_start(1, 6); break; /* BLACK: one long */
    }
}

/* --------------------------------------------------- Result banner -------- */

/*
 * The Result screen XML is authored with the RED variant hardcoded (banner
 * colour, badge icon, "MERAH - IMMEDIATE"), so without this the screen claimed
 * MERAH no matter what the SVM decided.
 */
static lv_color_t priority_color(ui_priority_t p)
{
    switch (p) {
    case UI_PRIORITY_RED:    return COLOR_DANGER;
    case UI_PRIORITY_YELLOW: return COLOR_PRIORITY_YELLOW;
    case UI_PRIORITY_GREEN:  return COLOR_PRIORITY_GREEN;
    default:                 return COLOR_BUTTONBAR_TOP; /* BLACK: near-black */
    }
}

static const void *priority_icon_src(ui_priority_t p)
{
    switch (p) {
    case UI_PRIORITY_RED:    return icon_priority_immediate;
    case UI_PRIORITY_YELLOW: return icon_priority_delayed;
    case UI_PRIORITY_GREEN:  return icon_priority_minor;
    default:                 return icon_close; /* no expectant icon exported */
    }
}

static void apply_priority(ui_priority_t p)
{
    lv_obj_t *banner;
    lv_obj_t *label;
    lv_obj_t *icon;

    if (result == NULL) {
        return;
    }
    banner = lv_obj_find_by_name(result, "result_banner");
    if (banner != NULL) {
        lv_obj_set_style_bg_color(banner, priority_color(p), 0);
    }
    label = lv_obj_find_by_name(result, "priority_label");
    if (label != NULL) {
        lv_label_set_text(label, ui_priority_display_label(p));
    }
    icon = lv_obj_find_by_name(result, "priority_icon");
    if (icon != NULL) {
        lv_image_set_src(icon, priority_icon_src(p));
        /* The style recolours the icon to red; match the banner instead. */
        lv_obj_set_style_image_recolor(icon, priority_color(p), 0);
    }
}

/*
 * Every screen was authored with placeholder text ("--", "ID Pasien: -"), so
 * nothing showed real readings until these ran. Set one `lbl_value` inside a
 * named result_vital tile.
 */
static void set_tile(lv_obj_t *root, const char *tile_name, const char *text)
{
    lv_obj_t *tile;
    lv_obj_t *label;

    if (root == NULL) {
        return;
    }
    tile = lv_obj_find_by_name(root, tile_name);
    if (tile == NULL) {
        return;
    }
    label = lv_obj_find_by_name(tile, "lbl_value");
    if (label != NULL) {
        lv_label_set_text(label, text);
    }
}

/* Fill the four vital tiles on any screen that has them. Both tile components
 * (result_vital, vital_card) name their readout label "lbl_value".
 *
 * Each tile is gated on its OWN validity bit, not on a whole-snapshot flag: one
 * unplugged sensor must not blank the three that are working.
 *
 * A tile whose bit is clear is REPAINTED to "--", not skipped. Skipping was the
 * same defect set_patient_id below documents: the authored "--" only survives
 * until the first good reading, after which a cleared bit left the last number
 * frozen on screen. So taking a finger off the sensor kept showing that finger's
 * SpO2, and it kept showing it for the next patient. A stale vital is worse than
 * no vital, because nothing about it looks stale. */
static void apply_vital_tiles(lv_obj_t *root, const vitals_t *v)
{
    char buf[16];

    if (root == NULL || v == NULL) {
        return;
    }
    if (v->valid_mask & UI_VITAL_SPO2) {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)v->spo2);
        set_tile(root, "vc_spo2", buf);
    } else {
        set_tile(root, "vc_spo2", "--");
    }
    if (v->valid_mask & UI_VITAL_HR) {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)v->hr);
        set_tile(root, "vc_hr", buf);
    } else {
        set_tile(root, "vc_hr", "--");
    }
    if (v->valid_mask & UI_VITAL_RR) {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)v->rr);
        set_tile(root, "vc_rr", buf);
    } else {
        set_tile(root, "vc_rr", "--");
    }
    if (v->valid_mask & UI_VITAL_BP) {
        lv_snprintf(buf, sizeof(buf), "%u/%u", (unsigned)v->bp_sys,
                    (unsigned)v->bp_dia);
        set_tile(root, "vc_bp", buf);
    } else {
        set_tile(root, "vc_bp", "--");
    }
}

/*
 * Writes unconditionally, including when there is no tag.
 *
 * The early "no tag -> return" this replaces had two failure modes on one line:
 * the label kept its authored "-" so a real ID never appeared, and a *previous*
 * patient's ID was never cleared -- which on a triage screen means attaching the
 * wrong identity to a set of vitals. Painting "--" is the safe default; it is
 * also the tell that this code ran at all, which "-" is not.
 *
 * UI_PATIENT_ID_PREFIX is printed here rather than carried in rfid_t.tag, so the
 * tag stays the bare card UID everywhere it is compared, hashed or sent. The
 * backend adds the identical prefix on ingest (normalizeRfid() in
 * src/common/mqtt-payload.ts) instead of it riding the LoRa packet, because a
 * constant costs airtime on every reading and the packet's RFID field has no
 * spare bytes. Change one side and you must change the other, or the operator
 * reads a different ID off this screen than the command post reads off the
 * dashboard.
 */
#define UI_PATIENT_ID_PREFIX "TB-"

static void set_patient_id(lv_obj_t *root, const char *prefix)
{
    const rfid_t *tag = ui_session_get_rfid();
    lv_obj_t *label;

    if (root == NULL) {
        return;
    }
    label = lv_obj_find_by_name(root, "patient_id");
    if (label == NULL) {
        /* Renaming a label in the XML would otherwise silently stop the ID from
         * ever updating again, with no symptom but a stale placeholder. */
        LV_LOG_WARN("no patient_id label on this screen");
        return;
    }
    if (tag != NULL && tag->present && tag->tag[0] != '\0') {
        lv_label_set_text_fmt(label, "%s" UI_PATIENT_ID_PREFIX "%s", prefix,
                              tag->tag);
    } else {
        /* No prefix on the placeholder: "TB---" reads like a malformed ID. */
        lv_label_set_text_fmt(label, "%s--", prefix);
    }
}

static void apply_result_vitals(void)
{
    apply_vital_tiles(result, ui_session_get_vitals());
    set_patient_id(result, "ID Pasien: ");
}

/* Mengukur: 4 tiles plus the progress bar and its percentage label. */
static void apply_mengukur(void)
{
    uint8_t pct = ui_session_get_measurement_progress();
    lv_obj_t *bar;
    lv_obj_t *label;

    if (mengukur == NULL) {
        return;
    }
    apply_vital_tiles(mengukur, ui_session_get_vitals());

    bar = lv_obj_find_by_name(mengukur, "measure_progress");
    if (bar != NULL) {
        lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    }
    label = lv_obj_find_by_name(mengukur, "measure_pct");
    if (label != NULL) {
        lv_label_set_text_fmt(label, "%u%%", (unsigned)pct);
    }
}

/*
 * Monitor uses its own per-vital labels (spo2_value/hr_value/rr_value) instead
 * of the shared tile component, so it needs its own pass.
 */
static void apply_monitor(void)
{
    const vitals_t *v = ui_session_get_vitals();
    static const struct { const char *name; uint8_t bit; } k_fields[] = {
        {"spo2_value", UI_VITAL_SPO2},
        {"hr_value",   UI_VITAL_HR},
        {"rr_value",   UI_VITAL_RR},
    };

    if (monitor == NULL || v == NULL) {
        return;
    }
    set_patient_id(monitor, "ID Pasien: ");
    for (unsigned i = 0; i < sizeof(k_fields) / sizeof(k_fields[0]); i++) {
        lv_obj_t *label;
        unsigned value;

        /* Per-field again: this is the live monitoring screen, so a working
         * sensor must keep updating while another is unplugged.
         *
         * Cleared bits are painted "--" rather than skipped, for the reason in
         * apply_vital_tiles: on the LIVE screen a skipped field freezes the last
         * number, so lifting a finger off the sensor leaves its SpO2 on display
         * as though it were still being measured. */
        label = lv_obj_find_by_name(monitor, k_fields[i].name);
        if (label == NULL) {
            continue;
        }
        if ((v->valid_mask & k_fields[i].bit) == 0U) {
            lv_label_set_text(label, "--");
            continue;
        }
        value = (k_fields[i].bit == UI_VITAL_SPO2) ? v->spo2
              : (k_fields[i].bit == UI_VITAL_HR)   ? v->hr : v->rr;
        lv_label_set_text_fmt(label, "%u", value);
    }
    apply_vital_tiles(monitor, v);
}

/* --------------------------------------------------- Result banner blink -- */

/*
 * The banner flashes in step with the buzzer so the same information reaches
 * eyes and ears together. RED keeps flashing for as long as Result is shown:
 * an immediate case should not stop asking for attention. The others blink
 * only while their beep pattern plays.
 *
 * Implemented as opacity on the banner, not a colour swap, so it works for
 * every priority colour without a second palette.
 */
#define BLINK_DIM LV_OPA_40

static void banner_set_dim(bool dim)
{
    lv_obj_t *banner;

    if (result == NULL) {
        return;
    }
    banner = lv_obj_find_by_name(result, "result_banner");
    if (banner != NULL) {
        lv_obj_set_style_bg_opa(banner, dim ? BLINK_DIM : LV_OPA_COVER, 0);
    }
}

static void blink_tick(void)
{
    static bool s_dim;
    static uint8_t s_phase;
    bool want_blink;

    if (ui_nav_current() != UI_SCREEN_RESULT || !ui_session_has_priority()) {
        if (s_dim) {
            s_dim = false;
            banner_set_dim(false);
        }
        return;
    }

    /* RED: never stops. Others: only while the buzzer pattern is running. */
    want_blink = (ui_session_get_priority() == UI_PRIORITY_RED) ||
                 (s_beep_pulses > 0) || s_beep_is_on;

    if (!want_blink) {
        if (s_dim) {
            s_dim = false;
            banner_set_dim(false);
        }
        return;
    }

    /* 8 ticks x 50 ms = 400 ms per half cycle; calm enough to read through. */
    if (++s_phase >= 8U) {
        s_phase = 0;
        s_dim = !s_dim;
        banner_set_dim(s_dim);
    }
}

/* ------------------------------------------------------ Idle blanking ----- */

/*
 * Backlight + RGB DMA are the two dominant loads, so blanking on idle is the
 * single biggest battery win available. No dimming: BL_EN is a digital expander
 * pin, not PWM (EXIO5 could dim, but it is behind the same I2C expander as the
 * GT911 touch, so bit-banging PWM there would fight the touch controller).
 *
 * History: this was disabled for a while because the board powered itself off a
 * few seconds after the screen went dark. The cause was never established -- the
 * SW6106 light-load theory does not survive the arithmetic, since blanking only
 * removes the backlight (~200-300 mA of ~500 mA) and the panel DMA keeps
 * running. Re-enabled now that the STM32 and the sensors draw from this board
 * too: the baseline load is much higher, so blanking moves the total far less.
 *
 * If the device starts switching itself off again, set this back to 0 -- that is
 * the whole revert.
 */
#ifndef IDLE_BLANK_MS
#define IDLE_BLANK_MS 30000U /* 0 = never blank */
#endif

static bool s_screen_on = true;

static void idle_tick(void)
{
    lv_display_t *disp = lv_display_get_default();
    bool idle;

    if (IDLE_BLANK_MS == 0U || disp == NULL) {
        return;
    }
    idle = lv_display_get_inactive_time(disp) > IDLE_BLANK_MS;

    /* Never blank mid-measurement or while monitoring: the operator is reading
     * the screen precisely when they are not touching it. */
    if (idle && (ui_nav_current() == UI_SCREEN_MENGUKUR ||
                 ui_nav_current() == UI_SCREEN_MONITOR)) {
        idle = false;
    }

    if (idle == s_screen_on) {
        s_screen_on = !idle;
        ui_board_backlight(s_screen_on);

        if (s_screen_on) {
            /*
             * Swallow the press that did the waking, so tapping a dark screen
             * turns the light on instead of pressing whatever happens to sit
             * under the finger -- Power and Abort are both on the ButtonBar.
             *
             * ponytail: this timer runs every 50 ms, so a tap shorter than that
             * can still land its click before we get here. Fine for a finger on
             * a 480x480 panel; if it ever matters, hook the indev directly
             * instead of polling inactivity.
             */
            lv_indev_t *indev = NULL;

            while ((indev = lv_indev_get_next(indev)) != NULL) {
                lv_indev_wait_release(indev);
            }
        }
    }
}

/* Beep once per triage result, when Result first has a priority to show. */
static void result_beep_tick(void)
{
    static bool s_announced;

    if (ui_nav_current() != UI_SCREEN_RESULT || !ui_session_has_priority()) {
        /* Re-arm on leaving Result so the next patient is announced too. */
        s_announced = false;
        return;
    }
    if (!s_announced) {
        s_announced = true;
        apply_priority(ui_session_get_priority());
        beep_for_priority(ui_session_get_priority());
    }
}

/*
 * Beep once when a scan succeeds. The Berhasil screen otherwise announces itself
 * only visually, and the operator is looking at the card reader, not the panel.
 *
 * Armed by SCANNING rather than by "Berhasil is showing", because Berhasil is
 * also reachable backwards from Age -- and a chirp on Back would mean "card
 * read" when nothing was read. Re-arming on SCANNING also means rescanning the
 * same card still beeps, which comparing tag strings would not.
 */
static void scan_beep_tick(void)
{
    static bool s_armed;
    const rfid_t *tag;

    if (ui_nav_current() == UI_SCREEN_SCANNING) {
        s_armed = true;
        return;
    }
    if (!s_armed || ui_nav_current() != UI_SCREEN_BERHASIL) {
        return;
    }
    tag = ui_session_get_rfid();
    if (tag == NULL || !tag->present || tag->tag[0] == '\0') {
        return; /* Stay armed: the ID may land a poll later than the screen. */
    }
    s_armed = false;
    beep_start(1, 3); /* One ~150 ms blip: not a click, not a priority pattern. */
}

/*
 * Push the latest readings into whichever screen is showing. Must be called
 * unconditionally: this used to live at the tail of result_beep_tick(), which
 * returns early unless the current screen is Result WITH a priority -- so
 * Monitor and Mengukur were never refreshed and sat at their authored "--"
 * while Result, reached later in the flow, worked fine.
 *
 * Only the current screen is touched; each helper skips a NULL root.
 */
static void refresh_current_screen(void)
{
    switch (ui_nav_current()) {
    case UI_SCREEN_RESULT:   apply_result_vitals(); break;
    case UI_SCREEN_MENGUKUR: apply_mengukur(); break;
    case UI_SCREEN_MONITOR:  apply_monitor(); break;
    case UI_SCREEN_BERHASIL: set_patient_id(berhasil, ""); break;
    default: break;
    }
}

static void selection_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_bindings_sync_selection();
    ui_bindings_sync_status_dots();
    sync_status_bar();
    poll_power_request();
    poll_menu_request();

    /* A result announcement outranks a button click: start it first so the
     * click beep cannot truncate the pattern. */
    result_beep_tick();
    scan_beep_tick();
    /* Vitals keep arriving, so refresh every tick rather than once. Separate
     * from result_beep_tick() because that one returns early off Result. */
    refresh_current_screen();
    if (ui_action_take_beep_request() && s_beep_pulses == 0 && !s_beep_is_on) {
        beep_start(1, 1);
    }
    beep_tick();
    blink_tick();
    idle_tick();
}

static void dot_opa_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void animate_scan_dot(lv_obj_t *dot, uint32_t delay_ms)
{
    lv_anim_t a;

    if (dot == NULL) {
        return;
    }
    lv_anim_init(&a);
    lv_anim_set_var(&a, dot);
    lv_anim_set_exec_cb(&a, dot_opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_reverse_duration(&a, 600);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

void ui_bindings_start_scan_animation(void)
{
    static const char *dot_names[3] = {"scan_dot0", "scan_dot1", "scan_dot2"};
    uint8_t i;

    if (scanning == NULL) {
        return;
    }
    for (i = 0; i < 3U; i++) {
        lv_obj_t *dot = lv_obj_find_by_name(scanning, dot_names[i]);
        animate_scan_dot(dot, (uint32_t)i * 200U);
    }
}

static void set_focus(lv_obj_t *root, const char *name, bool on)
{
    lv_obj_t *opt = (root != NULL) ? lv_obj_find_by_name(root, name) : NULL;

    if (opt == NULL) {
        return;
    }
    if (on) {
        lv_obj_add_state(opt, LV_STATE_FOCUSED);
    } else {
        lv_obj_remove_state(opt, LV_STATE_FOCUSED);
    }
}

void ui_bindings_sync_selection(void)
{
    ui_age_band_t band = ui_nav_pending_age();
    ui_gender_t g = ui_nav_pending_gender();

    set_focus(age, "opt_6_17",    band == UI_AGE_BAND_6_17);
    set_focus(age, "opt_18_45",   band == UI_AGE_BAND_18_45);
    set_focus(age, "opt_46_60",   band == UI_AGE_BAND_46_60);
    set_focus(age, "opt_60_plus", band == UI_AGE_BAND_OVER_60);

    set_focus(gender, "opt_male",   g == UI_GENDER_M);
    set_focus(gender, "opt_female", g == UI_GENDER_F);

    {
        lv_obj_t *airway = ui_airway_screen();
        bool problem = ui_nav_pending_airway();

        set_focus(airway, "opt_airway_no",  !problem);
        set_focus(airway, "opt_airway_yes",  problem);
    }
}

void ui_bindings_init(void)
{
    uint8_t i;

    for (i = 0; i < UI_SCREEN_COUNT; i++) {
        bind_button_bar(k_screen_ids[i]);
    }

    bind_option(age, "opt_6_17",    age_option_cb, (uintptr_t)UI_AGE_BAND_6_17);
    bind_option(age, "opt_18_45",   age_option_cb, (uintptr_t)UI_AGE_BAND_18_45);
    bind_option(age, "opt_46_60",   age_option_cb, (uintptr_t)UI_AGE_BAND_46_60);
    bind_option(age, "opt_60_plus", age_option_cb, (uintptr_t)UI_AGE_BAND_OVER_60);

    bind_option(gender, "opt_male",   gender_option_cb, (uintptr_t)UI_GENDER_M);
    bind_option(gender, "opt_female", gender_option_cb, (uintptr_t)UI_GENDER_F);

    {
        lv_obj_t *airway = ui_airway_screen();

        bind_option(airway, "opt_airway_no",  airway_option_cb, (uintptr_t)0);
        bind_option(airway, "opt_airway_yes", airway_option_cb, (uintptr_t)1);
    }

    ui_bindings_sync_selection();
    ui_bindings_start_scan_animation();
    lv_timer_create(selection_timer_cb, 50, NULL);
}
