#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "ui_bindings.h"
#include "../ui.h"
#include "ui_action.h"
#include "ui_nav.h"

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

static void selection_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_bindings_sync_selection();
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
