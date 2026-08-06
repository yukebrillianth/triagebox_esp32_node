/**
 * @file age_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "age_gen.h"
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

lv_obj_t * age_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_age_root;
    static lv_style_t style_age_content;
    static lv_style_t style_age_title;
    static lv_style_t style_option_row;
    static lv_style_t style_option_row_focused;
    static lv_style_t style_option_label;
    static lv_style_t style_option_sub;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_age_root);
        lv_style_init(&style_age_content);
        lv_style_init(&style_age_title);
        lv_style_init(&style_option_row);
        lv_style_init(&style_option_row_focused);
        lv_style_init(&style_option_label);
        lv_style_init(&style_option_sub);

        lv_style_set_bg_color(&style_age_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_age_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_age_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_age_root, 0);
        lv_style_set_pad_gap(&style_age_root, 0);
        lv_style_set_pad_hor(&style_age_content, 40);
        lv_style_set_pad_ver(&style_age_content, 0);
        lv_style_set_text_color(&style_age_title, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_age_title, font_inter_semi_bold_18);
        lv_style_set_text_align(&style_age_title, LV_TEXT_ALIGN_CENTER);
        lv_style_set_width(&style_option_row, 400);
        lv_style_set_height(&style_option_row, 69);
        lv_style_set_bg_color(&style_option_row, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_option_row, (255 * 100 / 100));
        lv_style_set_radius(&style_option_row, RADIUS_DEFAULT);
        lv_style_set_pad_hor(&style_option_row, 16);
        lv_style_set_pad_ver(&style_option_row, 8);
        lv_style_set_border_width(&style_option_row, 0);
        lv_style_set_outline_width(&style_option_row, 0);
        lv_style_set_shadow_width(&style_option_row, 0);
        lv_style_set_text_color(&style_option_row, COLOR_DARK_TEXT);
        lv_style_set_bg_color(&style_option_row_focused, COLOR_ACCENT);
        lv_style_set_bg_opa(&style_option_row_focused, (255 * 100 / 100));
        lv_style_set_text_color(&style_option_row_focused, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_option_label, font_inter_bold_18);
        lv_style_set_text_align(&style_option_label, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_font(&style_option_sub, font_inter_regular_12);
        lv_style_set_text_opa(&style_option_sub, (255 * 70 / 100));
        lv_style_set_text_align(&style_option_sub, LV_TEXT_ALIGN_CENTER);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (age == NULL) age = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = age;
        lv_obj_set_name_static(lv_obj_0, "age_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_age_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 0, 10, 1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_add_style(content, &style_age_content, 0);
        lv_obj_t * lv_label_0 = lv_label_create(content);
        lv_label_set_text(lv_label_0, "Pilih Rentang Usia");
        lv_obj_set_width(lv_label_0, lv_pct(100));
        lv_obj_add_style(lv_label_0, &style_age_title, 0);

        lv_obj_t * opt_6_17 = column_create(content, 0, 2, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(opt_6_17, "opt_6_17");
        lv_obj_set_flag(opt_6_17, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(opt_6_17, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(opt_6_17, &style_option_row, 0);
        lv_obj_add_style(opt_6_17, &style_option_row_focused, LV_STATE_FOCUSED);
        lv_obj_t * lv_label_1 = lv_label_create(opt_6_17);
        lv_label_set_text(lv_label_1, "6-17 Tahun");
        lv_obj_set_width(lv_label_1, lv_pct(100));
        lv_obj_add_style(lv_label_1, &style_option_label, 0);

        lv_obj_t * lv_label_2 = lv_label_create(opt_6_17);
        lv_label_set_text(lv_label_2, "Anak-anak & Remaja");
        lv_obj_set_width(lv_label_2, lv_pct(100));
        lv_obj_add_style(lv_label_2, &style_option_sub, 0);

        lv_obj_t * opt_18_45 = column_create(content, 0, 2, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(opt_18_45, "opt_18_45");
        lv_obj_set_flag(opt_18_45, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(opt_18_45, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(opt_18_45, &style_option_row, 0);
        lv_obj_add_style(opt_18_45, &style_option_row_focused, LV_STATE_FOCUSED);
        lv_obj_t * lv_label_3 = lv_label_create(opt_18_45);
        lv_label_set_text(lv_label_3, "18-45 Tahun");
        lv_obj_set_width(lv_label_3, lv_pct(100));
        lv_obj_add_style(lv_label_3, &style_option_label, 0);

        lv_obj_t * lv_label_4 = lv_label_create(opt_18_45);
        lv_label_set_text(lv_label_4, "Dewasa");
        lv_obj_set_width(lv_label_4, lv_pct(100));
        lv_obj_add_style(lv_label_4, &style_option_sub, 0);

        lv_obj_t * opt_46_60 = column_create(content, 0, 2, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(opt_46_60, "opt_46_60");
        lv_obj_set_flag(opt_46_60, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(opt_46_60, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(opt_46_60, &style_option_row, 0);
        lv_obj_add_style(opt_46_60, &style_option_row_focused, LV_STATE_FOCUSED);
        lv_obj_t * lv_label_5 = lv_label_create(opt_46_60);
        lv_label_set_text(lv_label_5, "46-60 tahun");
        lv_obj_set_width(lv_label_5, lv_pct(100));
        lv_obj_add_style(lv_label_5, &style_option_label, 0);

        lv_obj_t * lv_label_6 = lv_label_create(opt_46_60);
        lv_label_set_text(lv_label_6, "Dewasa Tua");
        lv_obj_set_width(lv_label_6, lv_pct(100));
        lv_obj_add_style(lv_label_6, &style_option_sub, 0);

        lv_obj_t * opt_60_plus = column_create(content, 0, 2, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(opt_60_plus, "opt_60_plus");
        lv_obj_set_flag(opt_60_plus, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(opt_60_plus, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(opt_60_plus, &style_option_row, 0);
        lv_obj_add_style(opt_60_plus, &style_option_row_focused, LV_STATE_FOCUSED);
        lv_obj_t * lv_label_7 = lv_label_create(opt_60_plus);
        lv_label_set_text(lv_label_7, ">60 tahun");
        lv_obj_set_width(lv_label_7, lv_pct(100));
        lv_obj_add_style(lv_label_7, &style_option_label, 0);

        lv_obj_t * lv_label_8 = lv_label_create(opt_60_plus);
        lv_label_set_text(lv_label_8, "Lansia");
        lv_obj_set_width(lv_label_8, lv_pct(100));
        lv_obj_add_style(lv_label_8, &style_option_sub, 0);

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

