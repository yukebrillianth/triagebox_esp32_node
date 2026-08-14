/**
 * @file home_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "home_gen.h"
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

lv_obj_t * home_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_home_root;
    static lv_style_t style_home_content;
    static lv_style_t style_home_logo;
    static lv_style_t style_home_subtitle;
    static lv_style_t style_home_intro;
    static lv_style_t style_home_hint;
    static lv_style_t style_home_hint_text;
    static lv_style_t style_home_hint_accent;
    static lv_style_t style_home_status_dot;
    static lv_style_t style_home_status_text;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_home_root);
        lv_style_init(&style_home_content);
        lv_style_init(&style_home_logo);
        lv_style_init(&style_home_subtitle);
        lv_style_init(&style_home_intro);
        lv_style_init(&style_home_hint);
        lv_style_init(&style_home_hint_text);
        lv_style_init(&style_home_hint_accent);
        lv_style_init(&style_home_status_dot);
        lv_style_init(&style_home_status_text);

        lv_style_set_bg_color(&style_home_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_home_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_home_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_home_root, 0);
        lv_style_set_pad_gap(&style_home_root, 0);
        lv_style_set_pad_hor(&style_home_content, 40);
        lv_style_set_pad_ver(&style_home_content, 0);
        lv_style_set_width(&style_home_logo, 249);
        lv_style_set_height(&style_home_logo, 58);
        lv_style_set_text_color(&style_home_subtitle, COLOR_ACCENT);
        lv_style_set_text_font(&style_home_subtitle, font_inter_semi_bold_18);
        lv_style_set_text_align(&style_home_subtitle, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_color(&style_home_intro, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_home_intro, font_inter_regular_14);
        lv_style_set_text_align(&style_home_intro, LV_TEXT_ALIGN_CENTER);
        lv_style_set_bg_color(&style_home_hint, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_home_hint, (255 * 100 / 100));
        lv_style_set_radius(&style_home_hint, RADIUS_DEFAULT);
        lv_style_set_pad_all(&style_home_hint, 16);
        lv_style_set_border_width(&style_home_hint, 0);
        lv_style_set_outline_width(&style_home_hint, 0);
        lv_style_set_shadow_width(&style_home_hint, 0);
        lv_style_set_text_color(&style_home_hint_text, COLOR_TEXT_ON_CARD);
        lv_style_set_text_font(&style_home_hint_text, font_inter_regular_14);
        lv_style_set_text_align(&style_home_hint_text, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_color(&style_home_hint_accent, COLOR_ACCENT);
        lv_style_set_text_font(&style_home_hint_accent, font_inter_semi_bold_14);
        lv_style_set_bg_color(&style_home_status_dot, COLOR_STATUS_OK);
        lv_style_set_bg_opa(&style_home_status_dot, (255 * 100 / 100));
        lv_style_set_radius(&style_home_status_dot, lv_pct(100));
        lv_style_set_width(&style_home_status_dot, 12);
        lv_style_set_height(&style_home_status_dot, 12);
        lv_style_set_text_color(&style_home_status_text, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_home_status_text, font_inter_regular_10);
        lv_style_set_text_align(&style_home_status_text, LV_TEXT_ALIGN_CENTER);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (home == NULL) home = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = home;
        lv_obj_set_name_static(lv_obj_0, "home_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_home_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * home_content = column_create(lv_obj_0, 0, 24, 1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(home_content, "home_content");
        lv_obj_set_width(home_content, lv_pct(100));
        lv_obj_add_style(home_content, &style_home_content, 0);
        lv_obj_t * lv_image_0 = lv_image_create(home_content);
        lv_image_set_src(lv_image_0, logo_light_for_dark);
        lv_obj_set_width(lv_image_0, 249);
        lv_obj_set_height(lv_image_0, 58);
        lv_image_set_inner_align(lv_image_0, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_0, &style_home_logo, 0);

        lv_obj_t * column_0 = column_create(home_content, 0, 8, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(column_0, lv_pct(100));
        lv_obj_t * lv_label_0 = lv_label_create(column_0);
        lv_label_set_text(lv_label_0, "Sistem Triase Cerdas");
        lv_obj_set_width(lv_label_0, lv_pct(100));
        lv_obj_add_style(lv_label_0, &style_home_subtitle, 0);

        lv_obj_t * lv_label_1 = lv_label_create(column_0);
        lv_label_set_text(lv_label_1, "Siap untuk memulai triase pasien bencana");
        lv_obj_set_width(lv_label_1, lv_pct(100));
        lv_obj_add_style(lv_label_1, &style_home_intro, 0);

        lv_obj_t * lv_obj_1 = lv_obj_create(home_content);
        lv_obj_set_flex_flow(lv_obj_1, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_flex_main_place(lv_obj_1, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(lv_obj_1, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_height(lv_obj_1, 48);
        lv_obj_set_width(lv_obj_1, 380);
        lv_obj_add_style(lv_obj_1, &style_home_hint, 0);
        lv_obj_t * lv_spangroup_0 = lv_spangroup_create(lv_obj_1);
        lv_obj_set_width(lv_spangroup_0, lv_pct(100));
        lv_obj_set_style_text_align(lv_spangroup_0, LV_TEXT_ALIGN_CENTER, 0);
        lv_span_t * lv_spangroup_span_0 = lv_spangroup_add_span(lv_spangroup_0);
        lv_spangroup_set_span_text(lv_spangroup_0, lv_spangroup_span_0, "Tekan ");
        lv_spangroup_set_span_style(lv_spangroup_0, lv_spangroup_span_0, &style_home_hint_text);
        lv_span_t * lv_spangroup_span_1 = lv_spangroup_add_span(lv_spangroup_0);
        lv_spangroup_set_span_text(lv_spangroup_0, lv_spangroup_span_1, "SCAN");
        lv_spangroup_set_span_style(lv_spangroup_0, lv_spangroup_span_1, &style_home_hint_accent);
        lv_span_t * lv_spangroup_span_2 = lv_spangroup_add_span(lv_spangroup_0);
        lv_spangroup_set_span_text(lv_spangroup_0, lv_spangroup_span_2, " untuk memindai gelang RFID pasien");
        lv_spangroup_set_span_style(lv_spangroup_0, lv_spangroup_span_2, &style_home_hint_text);

        lv_obj_t * row_0 = row_create(home_content, 0, 16, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_margin_top(row_0, 32, 0);
        lv_obj_t * column_1 = column_create(row_0, 0, 6, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t * stat_dot_sys = lv_obj_create(column_1);
        lv_obj_set_name(stat_dot_sys, "stat_dot_sys");
        lv_obj_add_style(stat_dot_sys, &style_home_status_dot, 0);

        lv_obj_t * stat_text_sys = lv_label_create(column_1);
        lv_obj_set_name(stat_text_sys, "stat_text_sys");
        lv_label_set_text(stat_text_sys, "Sistem OK");
        lv_obj_set_width(stat_text_sys, LV_SIZE_CONTENT);
        lv_obj_set_height(stat_text_sys, 15);
        lv_obj_add_style(stat_text_sys, &style_home_status_text, 0);

        lv_obj_t * column_2 = column_create(row_0, 0, 6, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t * stat_dot_sensor = lv_obj_create(column_2);
        lv_obj_set_name(stat_dot_sensor, "stat_dot_sensor");
        lv_obj_add_style(stat_dot_sensor, &style_home_status_dot, 0);

        lv_obj_t * stat_text_sensor = lv_label_create(column_2);
        lv_obj_set_name(stat_text_sensor, "stat_text_sensor");
        lv_label_set_text(stat_text_sensor, "Sensor OK");
        lv_obj_set_width(stat_text_sensor, LV_SIZE_CONTENT);
        lv_obj_set_height(stat_text_sensor, 15);
        lv_obj_add_style(stat_text_sensor, &style_home_status_text, 0);

        lv_obj_t * column_3 = column_create(row_0, 0, 6, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t * stat_dot_lora = lv_obj_create(column_3);
        lv_obj_set_name(stat_dot_lora, "stat_dot_lora");
        lv_obj_add_style(stat_dot_lora, &style_home_status_dot, 0);

        lv_obj_t * stat_text_lora = lv_label_create(column_3);
        lv_obj_set_name(stat_text_lora, "stat_text_lora");
        lv_label_set_text(stat_text_lora, "LoRa OK");
        lv_obj_set_width(stat_text_lora, LV_SIZE_CONTENT);
        lv_obj_set_height(stat_text_lora, 15);
        lv_obj_add_style(stat_text_lora, &style_home_status_text, 0);

        lv_obj_t * button_bar_0 = button_bar_create(lv_obj_0, "", "Scan", "Power", "Menu", NULL, icon_search, icon_power, icon_menu, COLOR_DARK_TEXT, COLOR_DARK_TEXT, COLOR_DANGER, COLOR_DARK_TEXT);
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

