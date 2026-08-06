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
    static lv_style_t style_big_card;
    static lv_style_t style_mid_card;
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
        lv_style_init(&style_big_card);
        lv_style_init(&style_mid_card);
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
        lv_style_set_pad_ver(&style_mon_content, 0);
        lv_style_set_bg_color(&style_live_dot, COLOR_STATUS_OK);
        lv_style_set_bg_opa(&style_live_dot, (255 * 100 / 100));
        lv_style_set_radius(&style_live_dot, lv_pct(100));
        lv_style_set_width(&style_live_dot, 12);
        lv_style_set_height(&style_live_dot, 12);
        lv_style_set_text_color(&style_mon_id, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_mon_id, font_inter_semi_bold_14);
        lv_style_set_width(&style_big_card, 440);
        lv_style_set_height(&style_big_card, 130);
        lv_style_set_width(&style_mid_card, 214);
        lv_style_set_height(&style_mid_card, 90);
        lv_style_set_width(&style_footer, 440);
        lv_style_set_height(&style_footer, 40);
        lv_style_set_bg_color(&style_footer, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_footer, (255 * 100 / 100));
        lv_style_set_radius(&style_footer, RADIUS_DEFAULT);
        lv_style_set_pad_hor(&style_footer, 16);
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

        lv_obj_t * content = column_create(lv_obj_0, 0, 10, 1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_add_style(content, &style_mon_content, 0);
        lv_obj_t * row_0 = row_create(content, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_0, lv_pct(100));
        lv_obj_t * lv_obj_1 = lv_obj_create(row_0);
        lv_obj_add_style(lv_obj_1, &style_live_dot, 0);

        lv_obj_t * patient_id = lv_label_create(row_0);
        lv_obj_set_name(patient_id, "patient_id");
        lv_label_set_text(patient_id, "ID Pasien: -");
        lv_obj_add_style(patient_id, &style_mon_id, 0);

        lv_obj_t * vc_spo2 = vital_card_create(content, icon_oxygen, COLOR_VITAL_SPO2, "--", "%", "SpO2");
        lv_obj_set_name(vc_spo2, "vc_spo2");
        lv_obj_add_style(vc_spo2, &style_big_card, 0);

        lv_obj_t * row_1 = row_create(content, 0, 12, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_1, lv_pct(100));
        lv_obj_t * vc_hr = vital_card_create(row_1, icon_heart_pulse, COLOR_VITAL_HR, "--", "bpm", "HR");
        lv_obj_set_name(vc_hr, "vc_hr");
        lv_obj_add_style(vc_hr, &style_mid_card, 0);

        lv_obj_t * vc_rr = vital_card_create(row_1, icon_respiratory, COLOR_VITAL_RR, "--", "/min", "RR");
        lv_obj_set_name(vc_rr, "vc_rr");
        lv_obj_add_style(vc_rr, &style_mid_card, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(content);
        lv_obj_set_flex_flow(lv_obj_2, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(lv_obj_2, 440);
        lv_obj_set_height(lv_obj_2, 40);
        lv_obj_add_style(lv_obj_2, &style_footer, 0);
        lv_obj_t * row_2 = row_create(lv_obj_2, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_t * lv_obj_3 = lv_obj_create(row_2);
        lv_obj_add_style(lv_obj_3, &style_live_dot, 0);

        lv_obj_t * lv_label_0 = lv_label_create(row_2);
        lv_label_set_text(lv_label_0, "Monitoring aktif");
        lv_obj_add_style(lv_label_0, &style_footer_text, 0);

        lv_obj_t * row_3 = row_create(lv_obj_2, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_t * lv_image_0 = lv_image_create(row_3);
        lv_image_set_src(lv_image_0, icon_clock);
        lv_obj_set_width(lv_image_0, 16);
        lv_obj_set_height(lv_image_0, 16);
        lv_image_set_inner_align(lv_image_0, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_0, &style_clock_icon, 0);

        lv_obj_t * update_ago = lv_label_create(row_3);
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

