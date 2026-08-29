#include "ui_airway.h"

#include <stddef.h>

#include "../ui.h"

/*
 * Styles are gender.xml's, transcribed. The point is that this screen is
 * indistinguishable from the two authored input screens beside it -- an operator
 * should not be able to tell that one of the three came from a different place.
 * Values live here rather than being #included from anywhere because the Editor
 * emits its styles as file-static in each *_gen.c; there is nothing to share.
 */
static lv_style_t s_root;
static lv_style_t s_content;
static lv_style_t s_title;
static lv_style_t s_row;
static lv_style_t s_row_focused;
static lv_style_t s_icon_bg;
static lv_style_t s_icon;
static lv_style_t s_label;
static bool s_styles_inited;

static lv_obj_t *s_screen;

static void init_styles(void)
{
    if (s_styles_inited) {
        return;
    }
    s_styles_inited = true;

    lv_style_init(&s_root);
    lv_style_set_bg_color(&s_root, COLOR_DARK_BG);
    lv_style_set_bg_opa(&s_root, LV_OPA_COVER);
    lv_style_set_text_color(&s_root, COLOR_DARK_TEXT);
    lv_style_set_pad_all(&s_root, 0);
    lv_style_set_pad_gap(&s_root, 0);

    lv_style_init(&s_content);
    lv_style_set_pad_hor(&s_content, 40);
    lv_style_set_pad_ver(&s_content, 0);

    lv_style_init(&s_title);
    lv_style_set_text_color(&s_title, COLOR_DARK_TEXT);
    lv_style_set_text_font(&s_title, font_inter_semi_bold_18);
    lv_style_set_text_align(&s_title, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&s_row);
    lv_style_set_width(&s_row, 400);
    lv_style_set_height(&s_row, 84);
    lv_style_set_bg_color(&s_row, COLOR_DARK_PANEL);
    lv_style_set_bg_opa(&s_row, LV_OPA_COVER);
    lv_style_set_radius(&s_row, 16);
    lv_style_set_pad_left(&s_row, 16);
    lv_style_set_pad_right(&s_row, 20);
    lv_style_set_pad_top(&s_row, 0);
    lv_style_set_pad_bottom(&s_row, 0);
    lv_style_set_border_width(&s_row, 0);
    lv_style_set_outline_width(&s_row, 0);
    lv_style_set_shadow_width(&s_row, 0);
    lv_style_set_text_color(&s_row, COLOR_DARK_TEXT);

    lv_style_init(&s_row_focused);
    lv_style_set_bg_color(&s_row_focused, COLOR_ACCENT);
    lv_style_set_bg_opa(&s_row_focused, LV_OPA_COVER);
    lv_style_set_text_color(&s_row_focused, COLOR_DARK_TEXT);

    lv_style_init(&s_icon_bg);
    lv_style_set_width(&s_icon_bg, 48);
    lv_style_set_height(&s_icon_bg, 48);
    lv_style_set_bg_color(&s_icon_bg, COLOR_ACCENT_TEXT);
    lv_style_set_bg_opa(&s_icon_bg, LV_OPA_COVER);
    lv_style_set_radius(&s_icon_bg, 100);
    lv_style_set_pad_all(&s_icon_bg, 0);
    lv_style_set_border_width(&s_icon_bg, 0);
    lv_style_set_outline_width(&s_icon_bg, 0);
    lv_style_set_shadow_width(&s_icon_bg, 0);

    lv_style_init(&s_icon);
    lv_style_set_image_recolor_opa(&s_icon, LV_OPA_COVER);

    lv_style_init(&s_label);
    lv_style_set_text_font(&s_label, font_inter_bold_20);
    lv_style_set_text_color(&s_label, COLOR_DARK_TEXT);
}

/*
 * One option row. `tint` is the icon colour -- unlike gender, where both options
 * are equally ordinary, one answer here means "this patient is RED regardless of
 * the model", so its icon is danger red and the other's is the accent. The row
 * background stays identical either way: colouring the whole row red would read
 * as "this row is selected/dangerous" before anybody has chosen anything.
 */
static void add_option(lv_obj_t *parent, const char *name, const void *icon,
                       lv_color_t tint, const char *text)
{
    lv_obj_t *row = row_create(parent, 0, 16, 0, LV_FLEX_ALIGN_START,
                               LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *circle;
    lv_obj_t *image;
    lv_obj_t *label;

    lv_obj_set_name(row, name);
    lv_obj_set_flag(row, LV_OBJ_FLAG_CLICKABLE, true);
    lv_obj_set_flag(row, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_style_pad_hor(row, 18, 0);
    lv_obj_add_style(row, &s_row, 0);
    lv_obj_add_style(row, &s_row_focused, LV_STATE_FOCUSED);

    circle = column_create(row, 0, 0, 0, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(circle, 48);
    lv_obj_set_height(circle, 48);
    lv_obj_set_flag(circle, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_add_style(circle, &s_icon_bg, 0);

    image = lv_image_create(circle);
    lv_image_set_src(image, icon);
    lv_obj_set_width(image, 24);
    lv_obj_set_height(image, 24);
    lv_obj_add_style(image, &s_icon, 0);
    lv_obj_set_style_image_recolor(image, tint, 0);

    label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_add_style(label, &s_label, 0);
}

lv_obj_t *ui_airway_screen(void)
{
    lv_obj_t *content;
    lv_obj_t *title;
    lv_obj_t *bar;

    if (s_screen != NULL) {
        return s_screen;
    }
    init_styles();

    s_screen = lv_obj_create(NULL);
    /* "airway_#" matches the naming the Editor gives screen roots, so
     * lv_obj_find_by_name() behaves the same way here as on the other eight. */
    lv_obj_set_name_static(s_screen, "airway_#");
    lv_obj_set_width(s_screen, 480);
    lv_obj_set_height(s_screen, 480);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_add_style(s_screen, &s_root, 0);

    /* Placeholder strings, exactly as the authored screens pass them:
     * sync_status_bar() in ui_bindings.c overwrites all four every second. */
    bar = status_bar_create(s_screen, battery_full, "80%", "Connected", "--:--");
    lv_obj_set_width(bar, 480);
    lv_obj_set_height(bar, 48);

    content = column_create(s_screen, 0, 14, 1, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_name(content, "content");
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flag(content, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_add_style(content, &s_content, 0);

    title = lv_label_create(content);
    /* A question, not a field name: this is a judgement the operator makes by
     * looking at the patient, not a reading, and the wording has to say so. */
    lv_label_set_text(title, "Ada Masalah Jalan Napas?");
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_add_style(title, &s_title, 0);

    /* "Tidak ada" first, and that ordering is deliberate: it is both the common
     * answer and the one pre-highlighted, so Up/Down starts where most patients
     * end up and the RED-forcing answer takes a deliberate move to reach. */
    add_option(content, "opt_airway_no", icon_check, COLOR_ACCENT,
               "Tidak ada");
    add_option(content, "opt_airway_yes", icon_respiratory, COLOR_DANGER,
               "Ada / tersumbat");

    bar = button_bar_create(s_screen, "Up", "Down", "Back", "Select",
                            icon_chevron_up, icon_chevron_down, icon_arrow_left,
                            icon_check, COLOR_DARK_TEXT, COLOR_DARK_TEXT,
                            COLOR_DARK_TEXT, COLOR_DARK_TEXT);
    lv_obj_set_width(bar, 480);
    lv_obj_set_height(bar, 71);

    return s_screen;
}
