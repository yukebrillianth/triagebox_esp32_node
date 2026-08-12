/**
 * @file monitor_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "monitor_gen.h"
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

lv_obj_t * monitor_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_mon_root;
    static lv_style_t style_mon_content;
    static lv_style_t style_live_dot;
    static lv_style_t style_mon_id;
    static lv_style_t style_mon_patient_row;
    static lv_style_t style_mon_card;
    static lv_style_t style_mon_hero_card;
    static lv_style_t style_mon_mid_card;
    static lv_style_t style_mon_hr_card;
    static lv_style_t style_mon_rr_card;
    static lv_style_t style_mon_header_label;
    static lv_style_t style_mon_icon;
    static lv_style_t style_mon_trend;
    static lv_style_t style_mon_hero_value;
    static lv_style_t style_mon_hero_unit;
    static lv_style_t style_mon_mid_value;
    static lv_style_t style_mon_mid_unit;
    static lv_style_t style_mon_spo2_track;
    static lv_style_t style_mon_spo2_indicator;
    static lv_style_t style_footer;
    static lv_style_t style_footer_text;
    static lv_style_t style_footer_muted;
    static lv_style_t style_clock_icon;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_mon_root);
        lv_style_init(&style_mon_content);
        lv_style_init(&style_live_dot);
        lv_style_init(&style_mon_id);
        lv_style_init(&style_mon_patient_row);
        lv_style_init(&style_mon_card);
        lv_style_init(&style_mon_hero_card);
        lv_style_init(&style_mon_mid_card);
        lv_style_init(&style_mon_hr_card);
        lv_style_init(&style_mon_rr_card);
        lv_style_init(&style_mon_header_label);
        lv_style_init(&style_mon_icon);
        lv_style_init(&style_mon_trend);
        lv_style_init(&style_mon_hero_value);
        lv_style_init(&style_mon_hero_unit);
        lv_style_init(&style_mon_mid_value);
        lv_style_init(&style_mon_mid_unit);
        lv_style_init(&style_mon_spo2_track);
        lv_style_init(&style_mon_spo2_indicator);
        lv_style_init(&style_footer);
        lv_style_init(&style_footer_text);
        lv_style_init(&style_footer_muted);
        lv_style_init(&style_clock_icon);

        lv_style_set_bg_color(&style_mon_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_mon_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_mon_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_mon_root, 0);
        lv_style_set_pad_gap(&style_mon_root, 0);
        lv_style_set_pad_hor(&style_mon_content, 20);
        lv_style_set_pad_ver(&style_mon_content, 20);
        lv_style_set_bg_color(&style_live_dot, COLOR_STATUS_OK);
        lv_style_set_bg_opa(&style_live_dot, (255 * 100 / 100));
        lv_style_set_radius(&style_live_dot, lv_pct(100));
        lv_style_set_width(&style_live_dot, 12);
        lv_style_set_height(&style_live_dot, 12);
        lv_style_set_text_color(&style_mon_id, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_mon_id, font_inter_semi_bold_14);
        lv_style_set_width(&style_mon_patient_row, lv_pct(100));
        lv_style_set_height(&style_mon_patient_row, 21);
        lv_style_set_bg_opa(&style_mon_card, (255 * 12 / 100));
        lv_style_set_radius(&style_mon_card, RADIUS_DEFAULT);
        lv_style_set_border_width(&style_mon_card, 2);
        lv_style_set_border_opa(&style_mon_card, (255 * 55 / 100));
        lv_style_set_outline_width(&style_mon_card, 0);
        lv_style_set_shadow_width(&style_mon_card, 0);
        lv_style_set_text_color(&style_mon_card, COLOR_DARK_TEXT);
        lv_style_set_width(&style_mon_hero_card, 440);
        lv_style_set_height(&style_mon_hero_card, 142);
        lv_style_set_pad_hor(&style_mon_hero_card, 16);
        lv_style_set_pad_ver(&style_mon_hero_card, 12);
        lv_style_set_bg_color(&style_mon_hero_card, COLOR_VITAL_SPO2);
        lv_style_set_border_color(&style_mon_hero_card, COLOR_VITAL_SPO2);
        lv_style_set_width(&style_mon_mid_card, 214);
        lv_style_set_height(&style_mon_mid_card, 93);
        lv_style_set_pad_hor(&style_mon_mid_card, 10);
        lv_style_set_pad_ver(&style_mon_mid_card, 10);
        lv_style_set_bg_color(&style_mon_hr_card, COLOR_VITAL_HR);
        lv_style_set_border_color(&style_mon_hr_card, COLOR_VITAL_HR);
        lv_style_set_bg_color(&style_mon_rr_card, COLOR_VITAL_RR);
        lv_style_set_border_color(&style_mon_rr_card, COLOR_VITAL_RR);
        lv_style_set_text_color(&style_mon_header_label, COLOR_TEXT_ON_CARD);
        lv_style_set_text_font(&style_mon_header_label, font_inter_semi_bold_14);
        lv_style_set_image_recolor_opa(&style_mon_icon, (255 * 100 / 100));
        lv_style_set_width(&style_mon_icon, 24);
        lv_style_set_height(&style_mon_icon, 24);
        lv_style_set_image_recolor_opa(&style_mon_trend, (255 * 100 / 100));
        lv_style_set_width(&style_mon_trend, 16);
        lv_style_set_height(&style_mon_trend, 16);
        lv_style_set_text_color(&style_mon_hero_value, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_mon_hero_value, font_inter_bold_56);
        lv_style_set_text_color(&style_mon_hero_unit, COLOR_TEXT_ON_CARD);
        lv_style_set_text_font(&style_mon_hero_unit, font_inter_regular_24);
        lv_style_set_text_color(&style_mon_mid_value, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_mon_mid_value, font_inter_bold_36);
        lv_style_set_text_color(&style_mon_mid_unit, COLOR_TEXT_ON_CARD);
        lv_style_set_text_font(&style_mon_mid_unit, font_inter_regular_14);
        lv_style_set_bg_color(&style_mon_spo2_track, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_mon_spo2_track, (255 * 100 / 100));
        lv_style_set_radius(&style_mon_spo2_track, 100);
        lv_style_set_border_width(&style_mon_spo2_track, 0);
        lv_style_set_outline_width(&style_mon_spo2_track, 0);
        lv_style_set_shadow_width(&style_mon_spo2_track, 0);
        lv_style_set_bg_color(&style_mon_spo2_indicator, COLOR_VITAL_SPO2);
        lv_style_set_bg_opa(&style_mon_spo2_indicator, (255 * 100 / 100));
        lv_style_set_radius(&style_mon_spo2_indicator, 100);
        lv_style_set_width(&style_footer, lv_pct(100));
        lv_style_set_height(&style_footer, 37);
        lv_style_set_bg_color(&style_footer, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_footer, (255 * 100 / 100));
        lv_style_set_radius(&style_footer, RADIUS_DEFAULT);
        lv_style_set_pad_hor(&style_footer, 0);
        lv_style_set_pad_ver(&style_footer, 0);
        lv_style_set_flex_cross_place(&style_footer, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_main_place(&style_footer, LV_FLEX_ALIGN_SPACE_BETWEEN);
        lv_style_set_border_width(&style_footer, 0);
        lv_style_set_outline_width(&style_footer, 0);
        lv_style_set_shadow_width(&style_footer, 0);
        lv_style_set_text_color(&style_footer_text, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_footer_text, font_inter_regular_13);
        lv_style_set_text_color(&style_footer_muted, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_footer_muted, font_inter_regular_12);
        lv_style_set_image_recolor(&style_clock_icon, COLOR_TEXT_SECONDARY);
        lv_style_set_image_recolor_opa(&style_clock_icon, (255 * 100 / 100));
        lv_style_set_width(&style_clock_icon, 16);
        lv_style_set_height(&style_clock_icon, 16);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (monitor == NULL) monitor = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = monitor;
        lv_obj_set_name_static(lv_obj_0, "monitor_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_mon_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 20, 0, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_set_height(content, 361);
        lv_obj_set_flag(content, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(content, &style_mon_content, 0);
        lv_obj_t * row_0 = row_create(content, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_0, lv_pct(100));
        lv_obj_set_height(row_0, 21);
        lv_obj_add_style(row_0, &style_mon_patient_row, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(row_0);
        lv_obj_add_style(lv_obj_1, &style_live_dot, 0);

        lv_obj_t * patient_id = lv_label_create(row_0);
        lv_obj_set_name(patient_id, "patient_id");
        lv_label_set_text(patient_id, "ID Pasien: -");
        lv_obj_add_style(patient_id, &style_mon_id, 0);

        lv_obj_t * vc_spo2 = lv_obj_create(content);
        lv_obj_set_name(vc_spo2, "vc_spo2");
        lv_obj_set_flex_flow(vc_spo2, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(vc_spo2, lv_pct(100));
        lv_obj_set_height(vc_spo2, 142);
        lv_obj_set_style_margin_top(vc_spo2, 12, 0);
        lv_obj_set_flag(vc_spo2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(vc_spo2, &style_mon_card, 0);
        lv_obj_add_style(vc_spo2, &style_mon_hero_card, 0);
        lv_obj_t * row_1 = row_create(vc_spo2, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_1, 404);
        lv_obj_set_height(row_1, 24);
        lv_obj_set_style_flex_main_place(row_1, LV_FLEX_ALIGN_SPACE_BETWEEN, 0);
        lv_obj_t * row_2 = row_create(row_1, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_2, LV_SIZE_CONTENT);
        lv_obj_set_height(row_2, 24);
        lv_obj_t * lv_image_0 = lv_image_create(row_2);
        lv_image_set_src(lv_image_0, icon_oxygen);
        lv_obj_set_width(lv_image_0, 24);
        lv_obj_set_height(lv_image_0, 24);
        lv_image_set_inner_align(lv_image_0, LV_IMAGE_ALIGN_CENTER);
        lv_obj_set_style_image_recolor(lv_image_0, COLOR_VITAL_SPO2, 0);
        lv_obj_add_style(lv_image_0, &style_mon_icon, 0);

        lv_obj_t * lv_label_0 = lv_label_create(row_2);
        lv_label_set_text(lv_label_0, "SpO2");
        lv_obj_add_style(lv_label_0, &style_mon_header_label, 0);

        lv_obj_t * row_3 = row_create(vc_spo2, 0, 6, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_3, 404);
        lv_obj_set_height(row_3, 64);
        lv_obj_set_style_margin_top(row_3, 8, 0);
        lv_obj_t * spo2_value = lv_label_create(row_3);
        lv_obj_set_name(spo2_value, "spo2_value");
        lv_label_set_text(spo2_value, "--");
        lv_obj_add_style(spo2_value, &style_mon_hero_value, 0);

        lv_obj_t * lv_label_1 = lv_label_create(row_3);
        lv_label_set_text(lv_label_1, "%");
        lv_obj_set_style_margin_bottom(lv_label_1, 7, 0);
        lv_obj_add_style(lv_label_1, &style_mon_hero_unit, 0);

        lv_obj_t * spo2_progress = lv_bar_create(vc_spo2);
        lv_obj_set_name(spo2_progress, "spo2_progress");
        lv_bar_set_value(spo2_progress, 0, false);
        lv_bar_set_min_value(spo2_progress, 0);
        lv_bar_set_max_value(spo2_progress, 100);
        lv_obj_set_width(spo2_progress, 404);
        lv_obj_set_height(spo2_progress, 4);
        lv_obj_set_style_margin_top(spo2_progress, 10, 0);
        lv_obj_add_style(spo2_progress, &style_mon_spo2_track, 0);
        lv_obj_add_style(spo2_progress, &style_mon_spo2_indicator, LV_PART_INDICATOR);

        lv_obj_t * row_4 = row_create(content, 0, 12, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_4, lv_pct(100));
        lv_obj_set_height(row_4, 93);
        lv_obj_set_style_margin_top(row_4, 12, 0);
        lv_obj_t * vc_hr = lv_obj_create(row_4);
        lv_obj_set_name(vc_hr, "vc_hr");
        lv_obj_set_flex_flow(vc_hr, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(vc_hr, 214);
        lv_obj_set_height(vc_hr, 93);
        lv_obj_set_flag(vc_hr, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(vc_hr, &style_mon_card, 0);
        lv_obj_add_style(vc_hr, &style_mon_mid_card, 0);
        lv_obj_add_style(vc_hr, &style_mon_hr_card, 0);
        lv_obj_t * row_5 = row_create(vc_hr, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_5, 190);
        lv_obj_set_height(row_5, 24);
        lv_obj_set_style_flex_main_place(row_5, LV_FLEX_ALIGN_SPACE_BETWEEN, 0);
        lv_obj_t * row_6 = row_create(row_5, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_6, LV_SIZE_CONTENT);
        lv_obj_set_height(row_6, 24);
        lv_obj_t * lv_image_1 = lv_image_create(row_6);
        lv_image_set_src(lv_image_1, icon_heart_pulse);
        lv_obj_set_width(lv_image_1, 24);
        lv_obj_set_height(lv_image_1, 24);
        lv_image_set_inner_align(lv_image_1, LV_IMAGE_ALIGN_CENTER);
        lv_obj_set_style_image_recolor(lv_image_1, COLOR_VITAL_HR, 0);
        lv_obj_add_style(lv_image_1, &style_mon_icon, 0);

        lv_obj_t * lv_label_2 = lv_label_create(row_6);
        lv_label_set_text(lv_label_2, "HR");
        lv_obj_add_style(lv_label_2, &style_mon_header_label, 0);

        lv_obj_t * row_7 = row_create(vc_hr, 0, 5, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_7, 190);
        lv_obj_set_height(row_7, 39);
        lv_obj_set_style_margin_top(row_7, 6, 0);
        lv_obj_t * hr_value = lv_label_create(row_7);
        lv_obj_set_name(hr_value, "hr_value");
        lv_label_set_text(hr_value, "--");
        lv_obj_add_style(hr_value, &style_mon_mid_value, 0);

        lv_obj_t * lv_label_3 = lv_label_create(row_7);
        lv_label_set_text(lv_label_3, "bpm");
        lv_obj_set_style_margin_bottom(lv_label_3, 4, 0);
        lv_obj_add_style(lv_label_3, &style_mon_mid_unit, 0);

        lv_obj_t * vc_rr = lv_obj_create(row_4);
        lv_obj_set_name(vc_rr, "vc_rr");
        lv_obj_set_flex_flow(vc_rr, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(vc_rr, 214);
        lv_obj_set_height(vc_rr, 93);
        lv_obj_set_flag(vc_rr, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(vc_rr, &style_mon_card, 0);
        lv_obj_add_style(vc_rr, &style_mon_mid_card, 0);
        lv_obj_add_style(vc_rr, &style_mon_rr_card, 0);
        lv_obj_t * row_8 = row_create(vc_rr, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_8, 190);
        lv_obj_set_height(row_8, 24);
        lv_obj_set_style_flex_main_place(row_8, LV_FLEX_ALIGN_SPACE_BETWEEN, 0);
        lv_obj_t * row_9 = row_create(row_8, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_9, LV_SIZE_CONTENT);
        lv_obj_set_height(row_9, 24);
        lv_obj_t * lv_image_2 = lv_image_create(row_9);
        lv_image_set_src(lv_image_2, icon_respiratory);
        lv_obj_set_width(lv_image_2, 24);
        lv_obj_set_height(lv_image_2, 24);
        lv_image_set_inner_align(lv_image_2, LV_IMAGE_ALIGN_CENTER);
        lv_obj_set_style_image_recolor(lv_image_2, COLOR_VITAL_RR, 0);
        lv_obj_add_style(lv_image_2, &style_mon_icon, 0);

        lv_obj_t * lv_label_4 = lv_label_create(row_9);
        lv_label_set_text(lv_label_4, "RR");
        lv_obj_add_style(lv_label_4, &style_mon_header_label, 0);

        lv_obj_t * row_10 = row_create(vc_rr, 0, 5, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_10, 190);
        lv_obj_set_height(row_10, 39);
        lv_obj_set_style_margin_top(row_10, 6, 0);
        lv_obj_t * rr_value = lv_label_create(row_10);
        lv_obj_set_name(rr_value, "rr_value");
        lv_label_set_text(rr_value, "--");
        lv_obj_add_style(rr_value, &style_mon_mid_value, 0);

        lv_obj_t * lv_label_5 = lv_label_create(row_10);
        lv_label_set_text(lv_label_5, "/min");
        lv_obj_set_style_margin_bottom(lv_label_5, 4, 0);
        lv_obj_add_style(lv_label_5, &style_mon_mid_unit, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(content);
        lv_obj_set_flex_flow(lv_obj_2, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(lv_obj_2, lv_pct(100));
        lv_obj_set_height(lv_obj_2, 37);
        lv_obj_set_style_pad_hor(lv_obj_2, 20, 0);
        lv_obj_set_style_margin_top(lv_obj_2, 12, 0);
        lv_obj_set_style_flex_main_place(lv_obj_2, LV_FLEX_ALIGN_SPACE_BETWEEN, 0);
        lv_obj_set_style_flex_cross_place(lv_obj_2, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(lv_obj_2, &style_footer, 0);
        lv_obj_t * row_11 = row_create(lv_obj_2, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_height(row_11, lv_pct(100));
        lv_obj_t * lv_obj_3 = lv_obj_create(row_11);
        lv_obj_add_style(lv_obj_3, &style_live_dot, 0);

        lv_obj_t * lv_label_6 = lv_label_create(row_11);
        lv_label_set_text(lv_label_6, "Monitoring aktif");
        lv_obj_add_style(lv_label_6, &style_footer_text, 0);

        lv_obj_t * row_12 = row_create(lv_obj_2, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_height(row_12, 37);
        lv_obj_t * lv_image_3 = lv_image_create(row_12);
        lv_image_set_src(lv_image_3, icon_update);
        lv_obj_set_width(lv_image_3, 16);
        lv_obj_set_height(lv_image_3, 16);
        lv_image_set_inner_align(lv_image_3, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_3, &style_clock_icon, 0);

        lv_obj_t * update_ago = lv_label_create(row_12);
        lv_obj_set_name(update_ago, "update_ago");
        lv_label_set_text(update_ago, "Update 5s lalu");
        lv_obj_add_style(update_ago, &style_footer_muted, 0);

        lv_obj_t * button_bar_0 = button_bar_create(lv_obj_0, "Back", "Stop", "Power", "Menu", icon_arrow_left, icon_pause, icon_power, icon_menu, COLOR_DARK_TEXT, COLOR_DANGER, COLOR_DANGER, COLOR_DARK_TEXT);
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

