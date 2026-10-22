#include "ui_rr.h"

#include <stddef.h>

#include "../ui.h"

/*
 * Styles are age.xml's, transcribed -- Age because it is the other four-row band
 * list, and four rows only fit at its 69 px height (gender/airway's 84 does not).
 * The circle styles have no Age counterpart, so those two come from gender.xml
 * like ui_airway.c's. Values live here rather than being #included from anywhere
 * because the Editor emits its styles as file-static in each *_gen.c; there is
 * nothing to share.
 */
static lv_style_t s_root;
static lv_style_t s_content;
static lv_style_t s_title;
static lv_style_t s_row;
static lv_style_t s_row_focused;
static lv_style_t s_icon_bg;
static lv_style_t s_icon;
static lv_style_t s_label;
static lv_style_t s_sub;
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
    lv_style_set_height(&s_row, 69);
    lv_style_set_bg_color(&s_row, COLOR_DARK_PANEL);
    lv_style_set_bg_opa(&s_row, LV_OPA_COVER);
    lv_style_set_radius(&s_row, RADIUS_DEFAULT);
    lv_style_set_pad_hor(&s_row, 16);
    lv_style_set_pad_ver(&s_row, 8);
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

    /* Age centres both of its label lines; here the circle owns the left edge,
     * so centring the text in what is left would leave it floating away from
     * the icon. Dropping text_align gives LTR left, which is what the two
     * icon-and-label screens (gender, airway) already look like. */
    lv_style_init(&s_label);
    lv_style_set_text_font(&s_label, font_inter_bold_18);

    lv_style_init(&s_sub);
    lv_style_set_text_font(&s_sub, font_inter_regular_12);
    lv_style_set_text_opa(&s_sub, LV_OPA_70);
}

/*
 * One option row. `tint` is the icon colour: under 12 and over 30 are the two
 * counts that should worry whoever is holding the box, so those get danger red
 * and the two middle bands get the accent. The row BACKGROUND stays identical
 * across all four -- same reasoning as ui_airway.c's add_option(): a red row
 * would read as "this row is selected/dangerous" before anybody has chosen
 * anything, and the focused style is the only thing allowed to say that.
 *
 * The sublabel's number is printed from ui_rr_band_value() (ui_types.c) instead
 * of being spelled out per row, so the screen cannot end up promising a figure
 * that is not the one handed to the model.
 */
static void add_option(lv_obj_t *parent, const char *name, ui_rr_band_t band,
                       lv_color_t tint, const char *text, const char *cue)
{
    char sub[48];
    lv_obj_t *row = row_create(parent, 0, 16, 0, LV_FLEX_ALIGN_START,
                               LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *circle;
    lv_obj_t *image;
    lv_obj_t *texts;
    lv_obj_t *label;

    lv_obj_set_name(row, name);
    lv_obj_set_flag(row, LV_OBJ_FLAG_CLICKABLE, true);
    lv_obj_set_flag(row, LV_OBJ_FLAG_SCROLLABLE, false);
    /* container_create() writes pad_all=0 onto the row as a LOCAL style, and a
     * local property outranks every lv_obj_add_style(), so the transcribed pad
     * is dead unless re-applied here -- which is why ui_airway.c re-applies its
     * own, and why Age's authored pads have no effect (its labels are centred
     * and full-width, so nobody noticed). Here the icon owns the left edge and
     * an unpadded row would put it against the corner radius. */
    lv_obj_set_style_pad_hor(row, 16, 0);
    lv_obj_set_style_pad_ver(row, 8, 0);
    lv_obj_add_style(row, &s_row, 0);
    lv_obj_add_style(row, &s_row_focused, LV_STATE_FOCUSED);

    circle = column_create(row, 0, 0, 0, LV_FLEX_ALIGN_CENTER,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(circle, 48);
    lv_obj_set_height(circle, 48);
    lv_obj_set_flag(circle, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_add_style(circle, &s_icon_bg, 0);

    image = lv_image_create(circle);
    lv_image_set_src(image, icon_respiratory);
    lv_obj_set_width(image, 24);
    lv_obj_set_height(image, 24);
    lv_obj_add_style(image, &s_icon, 0);
    lv_obj_set_style_image_recolor(image, tint, 0);

    texts = column_create(row, 0, 2, 1, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_flag(texts, LV_OBJ_FLAG_SCROLLABLE, false);

    label = lv_label_create(texts);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &s_label, 0);

    lv_snprintf(sub, sizeof(sub), "%s - dicatat %u/menit", cue,
                (unsigned)ui_rr_band_value(band));
    label = lv_label_create(texts);
    lv_label_set_text(label, sub);
    lv_obj_add_style(label, &s_sub, 0);
}

lv_obj_t *ui_rr_screen(void)
{
    lv_obj_t *content;
    lv_obj_t *title;
    lv_obj_t *bar;

    if (s_screen != NULL) {
        return s_screen;
    }
    init_styles();

    s_screen = lv_obj_create(NULL);
    /* "rr_#" matches the naming the Editor gives screen roots, so
     * lv_obj_find_by_name() behaves the same way here as on the other eight. */
    lv_obj_set_name_static(s_screen, "rr_#");
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

    /* gap 10, Age's, not airway's 14: title + 4x69 + 4 gaps has to fit the
     * 361 px between the two bars, and 14 leaves only 7 px of slack. */
    content = column_create(s_screen, 0, 10, 1, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_name(content, "content");
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flag(content, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_add_style(content, &s_content, 0);

    title = lv_label_create(content);
    /* Not a question like Airway's -- this is a count, so the title names the
     * field and carries the unit, because the rows themselves are bare numbers
     * and "12 - 20" of anything is not an instruction. mengukur's tile can put
     * "/min" under the value; here the unit has nowhere else to live. */
    lv_label_set_text(title, "Laju Pernapasan (napas/menit)");
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_add_style(title, &s_title, 0);

    /* Bands ascending, so the list reads like the scale it is. 12-20 is the
     * normal band and the one ui_nav should start on: the common answer costs no
     * presses, and either abnormal band takes a deliberate move to reach. */
    add_option(content, "opt_rr_under_12", UI_RR_BAND_UNDER_12, COLOR_DANGER,
               "< 12", "Lambat");
    add_option(content, "opt_rr_12_20", UI_RR_BAND_12_20, COLOR_ACCENT,
               "12 - 20", "Normal");
    add_option(content, "opt_rr_21_30", UI_RR_BAND_21_30, COLOR_ACCENT,
               "21 - 30", "Cepat");
    add_option(content, "opt_rr_over_30", UI_RR_BAND_OVER_30, COLOR_DANGER,
               "> 30", "Sangat cepat");

    bar = button_bar_create(s_screen, "Up", "Down", "Back", "Select",
                            icon_chevron_up, icon_chevron_down, icon_arrow_left,
                            icon_check, COLOR_DARK_TEXT, COLOR_DARK_TEXT,
                            COLOR_DARK_TEXT, COLOR_DARK_TEXT);
    lv_obj_set_width(bar, 480);
    lv_obj_set_height(bar, 71);

    return s_screen;
}
