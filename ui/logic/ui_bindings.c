#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "ui_bindings.h"
#include "../ui.h"
#include "ui_action.h"
#include "ui_board.h"
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

/* ------------------------------------------------------ Power confirm ----- */

/*
 * Hand-built instead of lv_msgbox: the stock msgbox brings LVGL's own theme
 * (light grey, its own radius and fonts) and looks nothing like the dark Figma
 * screens. Tokens below come from ui/globals.xml via ui_gen.h — no raw hex.
 */
static lv_obj_t *s_power_dialog;

static void power_confirm_cb(lv_event_t *e)
{
    bool confirmed = (bool)(uintptr_t)lv_event_get_user_data(e);

    if (s_power_dialog != NULL) {
        lv_obj_delete(s_power_dialog);
        s_power_dialog = NULL;
    }
    if (confirmed) {
        ui_mock_power_off();
    }
}

/* One dialog button: 130x48, square, ButtonBar-style gradient + 1px border. */
static lv_obj_t *dialog_button(lv_obj_t *parent, const char *text, bool danger,
                               bool confirmed)
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

    lv_obj_add_event_cb(btn, power_confirm_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)(confirmed ? 1 : 0));
    return btn;
}

static void poll_power_request(void)
{
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *body;
    lv_obj_t *row;
    bool measuring;

    if (!ui_action_take_power_request()) {
        return;
    }
    if (s_power_dialog != NULL) {
        return; /* already asking */
    }
    measuring = (ui_nav_current() == UI_SCREEN_MENGUKUR);

    /* Full-screen scrim: dims the screen behind and swallows stray touches so
     * the ButtonBar underneath cannot be pressed while the dialog is up. */
    s_power_dialog = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_power_dialog);
    lv_obj_set_size(s_power_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_power_dialog, COLOR_DARK_BG, 0);
    lv_obj_set_style_bg_opa(s_power_dialog, LV_OPA_70, 0);
    lv_obj_remove_flag(s_power_dialog, LV_OBJ_FLAG_SCROLLABLE);

    card = lv_obj_create(s_power_dialog);
    lv_obj_set_size(card, 400, 240);
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

    title = lv_label_create(card);
    lv_label_set_text(title, "Matikan alat?");
    lv_obj_set_style_text_color(title, COLOR_DARK_TEXT, 0);
    if (font_inter_bold_24 != NULL) {
        lv_obj_set_style_text_font(title, font_inter_bold_24, 0);
    }

    body = lv_label_create(card);
    lv_label_set_text(body, measuring
                      ? "Pengukuran sedang berjalan.\nData pasien akan hilang."
                      : "Alat akan dimatikan.");
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    /* Amber, not plain grey, when confirming actually destroys something. */
    lv_obj_set_style_text_color(body, measuring ? COLOR_STATUS_WARN
                                                : COLOR_TEXT_ON_CARD, 0);
    if (font_inter_regular_16 != NULL) {
        lv_obj_set_style_text_font(body, font_inter_regular_16, 0);
    }

    row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, SPACE_LG, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Cancel first so the leftmost, easiest target is the safe one. */
    dialog_button(row, "Batal", false, false);
    dialog_button(row, "Matikan", true, true);
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
 * (result_vital, vital_card) name their readout label "lbl_value". */
static void apply_vital_tiles(lv_obj_t *root, const vitals_t *v)
{
    char buf[16];

    if (root == NULL || v == NULL || !v->valid) {
        return; /* keep the authored "--" rather than invent a reading */
    }
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)v->spo2);
    set_tile(root, "vc_spo2", buf);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)v->hr);
    set_tile(root, "vc_hr", buf);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)v->rr);
    set_tile(root, "vc_rr", buf);
    lv_snprintf(buf, sizeof(buf), "%u/%u", (unsigned)v->bp_sys, (unsigned)v->bp_dia);
    set_tile(root, "vc_bp", buf);
}

static void set_patient_id(lv_obj_t *root, const char *prefix)
{
    const rfid_t *tag = ui_session_get_rfid();
    lv_obj_t *label;

    if (root == NULL || tag == NULL || !tag->present) {
        return;
    }
    label = lv_obj_find_by_name(root, "patient_id");
    if (label != NULL) {
        lv_label_set_text_fmt(label, "%s%s", prefix, tag->tag);
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
    static const struct { const char *name; int field; } k_fields[] = {
        {"spo2_value", 0}, {"hr_value", 1}, {"rr_value", 2},
    };

    if (monitor == NULL || v == NULL) {
        return;
    }
    set_patient_id(monitor, "ID Pasien: ");
    if (!v->valid) {
        return;
    }
    for (unsigned i = 0; i < sizeof(k_fields) / sizeof(k_fields[0]); i++) {
        lv_obj_t *label = lv_obj_find_by_name(monitor, k_fields[i].name);
        unsigned value = (k_fields[i].field == 0) ? v->spo2
                       : (k_fields[i].field == 1) ? v->hr : v->rr;
        if (label != NULL) {
            lv_label_set_text_fmt(label, "%u", value);
        }
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
 * single biggest battery win available (roughly 2.5 W -> 0.7 W). No dimming:
 * BL_EN is a digital expander pin, not PWM.
 *
 * DISABLED BY DEFAULT: on this board the saving is large enough to trip the
 * SW6106 power-bank IC's light-load auto-off, which switches the whole device
 * off a few seconds after the screen goes dark (the USB port disappears too).
 * That is a documented board quirk, not a firmware bug -- see
 * docs/firmware-architecture.md. Re-enable once the SW6106 path is settled
 * (test on battery, or keep a residual load).
 */
#ifndef IDLE_BLANK_MS
#define IDLE_BLANK_MS 0U /* 0 = never blank; 30000 = 30 s */
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
    /* Vitals keep arriving, so refresh every tick rather than once. Only the
     * current screen is touched; the others are skipped inside each helper. */
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
    poll_power_request();

    /* A result announcement outranks a button click: start it first so the
     * click beep cannot truncate the pattern. */
    result_beep_tick();
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

    ui_bindings_sync_selection();
    ui_bindings_start_scan_animation();
    lv_timer_create(selection_timer_cb, 50, NULL);
}
