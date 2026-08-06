/**
 * @file gender_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "gender_gen.h"
#include "../ui.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * gender_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_gender_root;
    static lv_style_t style_gender_content;
    static lv_style_t style_gender_title;
    static lv_style_t style_gender_row;
    static lv_style_t style_gender_row_focused;
    static lv_style_t style_gender_icon_bg;
    static lv_style_t style_gender_icon;
    static lv_style_t style_gender_label;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_gender_root);
        lv_style_init(&style_gender_content);
        lv_style_init(&style_gender_title);
        lv_style_init(&style_gender_row);
        lv_style_init(&style_gender_row_focused);
        lv_style_init(&style_gender_icon_bg);
        lv_style_init(&style_gender_icon);
        lv_style_init(&style_gender_label);

        lv_style_set_bg_color(&style_gender_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_gender_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_gender_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_gender_root, 0);
        lv_style_set_pad_gap(&style_gender_root, 0);
        lv_style_set_pad_hor(&style_gender_content, 40);
        lv_style_set_pad_ver(&style_gender_content, 0);
        lv_style_set_text_color(&style_gender_title, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_gender_title, font_inter_semi_bold_18);
        lv_style_set_text_align(&style_gender_title, LV_TEXT_ALIGN_CENTER);
        lv_style_set_width(&style_gender_row, 400);
        lv_style_set_height(&style_gender_row, 84);
        lv_style_set_bg_color(&style_gender_row, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_gender_row, (255 * 100 / 100));
        lv_style_set_radius(&style_gender_row, 16);
        lv_style_set_pad_left(&style_gender_row, 16);
        lv_style_set_pad_right(&style_gender_row, 20);
        lv_style_set_pad_top(&style_gender_row, 0);
        lv_style_set_pad_bottom(&style_gender_row, 0);
        lv_style_set_border_width(&style_gender_row, 0);
        lv_style_set_outline_width(&style_gender_row, 0);
        lv_style_set_shadow_width(&style_gender_row, 0);
        lv_style_set_text_color(&style_gender_row, COLOR_DARK_TEXT);
        lv_style_set_bg_color(&style_gender_row_focused, COLOR_ACCENT);
        lv_style_set_bg_opa(&style_gender_row_focused, (255 * 100 / 100));
        lv_style_set_text_color(&style_gender_row_focused, COLOR_DARK_TEXT);
        lv_style_set_width(&style_gender_icon_bg, 48);
        lv_style_set_height(&style_gender_icon_bg, 48);
        lv_style_set_bg_color(&style_gender_icon_bg, COLOR_ACCENT_TEXT);
        lv_style_set_bg_opa(&style_gender_icon_bg, (255 * 100 / 100));
        lv_style_set_radius(&style_gender_icon_bg, 100);
        lv_style_set_pad_all(&style_gender_icon_bg, 0);
        lv_style_set_border_width(&style_gender_icon_bg, 0);
        lv_style_set_outline_width(&style_gender_icon_bg, 0);
        lv_style_set_shadow_width(&style_gender_icon_bg, 0);
        lv_style_set_image_recolor(&style_gender_icon, COLOR_ACCENT);
        lv_style_set_image_recolor_opa(&style_gender_icon, (255 * 100 / 100));
        lv_style_set_text_font(&style_gender_label, font_inter_bold_20);
        lv_style_set_text_color(&style_gender_label, COLOR_DARK_TEXT);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (gender == NULL) gender = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = gender;
        lv_obj_set_name_static(lv_obj_0, "gender_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_gender_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 0, 14, 1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_set_flag(content, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(content, &style_gender_content, 0);
        lv_obj_t * lv_label_0 = lv_label_create(content);
        lv_label_set_text(lv_label_0, "Pilih Jenis Kelamin");
        lv_obj_set_width(lv_label_0, lv_pct(100));
        lv_obj_add_style(lv_label_0, &style_gender_title, 0);

        lv_obj_t * opt_male = row_create(content, 0, 16, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_name(opt_male, "opt_male");
        lv_obj_set_flag(opt_male, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(opt_male, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_pad_hor(opt_male, 18, 0);
        lv_obj_add_style(opt_male, &style_gender_row, 0);
        lv_obj_add_style(opt_male, &style_gender_row_focused, LV_STATE_FOCUSED);
        lv_obj_t * column_0 = column_create(opt_male, 0, 0, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(column_0, 48);
        lv_obj_set_height(column_0, 48);
        lv_obj_set_flag(column_0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(column_0, &style_gender_icon_bg, 0);
        lv_obj_t * lv_image_0 = lv_image_create(column_0);
        lv_image_set_src(lv_image_0, icon_lock);
        lv_obj_set_width(lv_image_0, 24);
        lv_obj_set_height(lv_image_0, 24);
        lv_obj_add_style(lv_image_0, &style_gender_icon, 0);

        lv_obj_t * lv_label_1 = lv_label_create(opt_male);
        lv_label_set_text(lv_label_1, "Laki-Laki");
        lv_obj_set_flex_grow(lv_label_1, 1);
        lv_obj_add_style(lv_label_1, &style_gender_label, 0);

        lv_obj_t * opt_female = row_create(content, 0, 16, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_name(opt_female, "opt_female");
        lv_obj_set_flag(opt_female, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(opt_female, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_pad_hor(opt_female, 18, 0);
        lv_obj_add_style(opt_female, &style_gender_row, 0);
        lv_obj_add_style(opt_female, &style_gender_row_focused, LV_STATE_FOCUSED);
        lv_obj_t * column_1 = column_create(opt_female, 0, 0, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(column_1, 48);
        lv_obj_set_height(column_1, 48);
        lv_obj_set_flag(column_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(column_1, &style_gender_icon_bg, 0);
        lv_obj_t * lv_image_1 = lv_image_create(column_1);
        lv_image_set_src(lv_image_1, icon_bluetooth);
        lv_obj_set_width(lv_image_1, 24);
        lv_obj_set_height(lv_image_1, 24);
        lv_obj_add_style(lv_image_1, &style_gender_icon, 0);

        lv_obj_t * lv_label_2 = lv_label_create(opt_female);
        lv_label_set_text(lv_label_2, "Perempuan");
        lv_obj_set_flex_grow(lv_label_2, 1);
        lv_obj_add_style(lv_label_2, &style_gender_label, 0);

        lv_obj_t * button_bar_0 = button_bar_create(lv_obj_0, "Up", "Down", "Back", "Select", icon_arrow_up, icon_arrow_down, icon_arrow_left, icon_check, COLOR_DARK_TEXT, COLOR_DARK_TEXT, COLOR_DARK_TEXT, COLOR_DARK_TEXT);
        lv_obj_set_width(button_bar_0, 480);
        lv_obj_set_height(button_bar_0, 71);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

