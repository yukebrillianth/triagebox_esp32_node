/**
 * @file result_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "result_gen.h"
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

lv_obj_t * result_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_result_root;
    static lv_style_t style_result_content;
    static lv_style_t style_result_banner;
    static lv_style_t style_warn_circle;
    static lv_style_t style_warn_icon;
    static lv_style_t style_result_label;
    static lv_style_t style_id_pill;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_result_root);
        lv_style_init(&style_result_content);
        lv_style_init(&style_result_banner);
        lv_style_init(&style_warn_circle);
        lv_style_init(&style_warn_icon);
        lv_style_init(&style_result_label);
        lv_style_init(&style_id_pill);

        lv_style_set_bg_color(&style_result_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_result_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_result_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_result_root, 0);
        lv_style_set_pad_gap(&style_result_root, 0);
        lv_style_set_pad_hor(&style_result_content, 20);
        lv_style_set_pad_ver(&style_result_content, 0);
        lv_style_set_pad_gap(&style_result_content, 16);
        lv_style_set_width(&style_result_banner, 440);
        lv_style_set_height(&style_result_banner, 225);
        lv_style_set_bg_color(&style_result_banner, COLOR_DANGER);
        lv_style_set_bg_opa(&style_result_banner, (255 * 100 / 100));
        lv_style_set_radius(&style_result_banner, RADIUS_DEFAULT);
        lv_style_set_pad_hor(&style_result_banner, 16);
        lv_style_set_pad_ver(&style_result_banner, 44);
        lv_style_set_pad_gap(&style_result_banner, 12);
        lv_style_set_flex_main_place(&style_result_banner, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_cross_place(&style_result_banner, LV_FLEX_ALIGN_CENTER);
        lv_style_set_border_width(&style_result_banner, 0);
        lv_style_set_outline_width(&style_result_banner, 0);
        lv_style_set_shadow_width(&style_result_banner, 0);
        lv_style_set_text_color(&style_result_banner, COLOR_DARK_TEXT);
        lv_style_set_bg_color(&style_warn_circle, COLOR_DARK_TEXT);
        lv_style_set_bg_opa(&style_warn_circle, (255 * 100 / 100));
        lv_style_set_radius(&style_warn_circle, lv_pct(100));
        lv_style_set_width(&style_warn_circle, 64);
        lv_style_set_height(&style_warn_circle, 64);
        lv_style_set_pad_all(&style_warn_circle, 0);
        lv_style_set_border_width(&style_warn_circle, 0);
        lv_style_set_outline_width(&style_warn_circle, 0);
        lv_style_set_shadow_width(&style_warn_circle, 0);
        lv_style_set_image_recolor(&style_warn_icon, COLOR_DANGER);
        lv_style_set_image_recolor_opa(&style_warn_icon, (255 * 100 / 100));
        lv_style_set_width(&style_warn_icon, 28);
        lv_style_set_height(&style_warn_icon, 28);
        lv_style_set_text_font(&style_result_label, font_inter_bold_24);
        lv_style_set_text_color(&style_result_label, COLOR_DARK_TEXT);
        lv_style_set_text_align(&style_result_label, LV_TEXT_ALIGN_CENTER);
        lv_style_set_bg_opa(&style_id_pill, (255 * 30 / 100));
        lv_style_set_bg_color(&style_id_pill, COLOR_DARK_BG);
        lv_style_set_radius(&style_id_pill, 100);
        lv_style_set_pad_hor(&style_id_pill, 12);
        lv_style_set_pad_ver(&style_id_pill, 4);
        lv_style_set_text_color(&style_id_pill, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_id_pill, font_inter_semi_bold_14);
        lv_style_set_border_width(&style_id_pill, 0);
        lv_style_set_outline_width(&style_id_pill, 0);
        lv_style_set_shadow_width(&style_id_pill, 0);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (result == NULL) result = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = result;
        lv_obj_set_name_static(lv_obj_0, "result_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_result_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 20, 16, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_set_height(content, 361);
        lv_obj_set_style_flex_main_place(content, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(content, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_flag(content, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(content, &style_result_content, 0);
        lv_obj_t * result_banner = lv_obj_create(content);
        lv_obj_set_name(result_banner, "result_banner");
        lv_obj_set_flex_flow(result_banner, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(result_banner, lv_pct(100));
        lv_obj_set_height(result_banner, 225);
        lv_obj_set_flag(result_banner, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(result_banner, &style_result_banner, 0);
        lv_obj_t * priority_badge = lv_obj_create(result_banner);
        lv_obj_set_name(priority_badge, "priority_badge");
        lv_obj_set_width(priority_badge, 64);
        lv_obj_set_height(priority_badge, 64);
        lv_obj_set_flag(priority_badge, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(priority_badge, &style_warn_circle, 0);
        lv_obj_t * priority_icon = lv_image_create(priority_badge);
        lv_obj_set_name(priority_icon, "priority_icon");
        lv_image_set_src(priority_icon, icon_priority_immediate);
        lv_obj_set_width(priority_icon, 28);
        lv_obj_set_height(priority_icon, 28);
        lv_obj_set_align(priority_icon, LV_ALIGN_CENTER);
        lv_obj_add_style(priority_icon, &style_warn_icon, 0);

        lv_obj_t * priority_label = lv_label_create(result_banner);
        lv_obj_set_name(priority_label, "priority_label");
        lv_label_set_text(priority_label, "MERAH - IMMEDIATE");
        lv_obj_set_width(priority_label, lv_pct(100));
        lv_obj_add_style(priority_label, &style_result_label, 0);

        lv_obj_t * lv_obj_1 = lv_obj_create(result_banner);
        lv_obj_set_flex_flow(lv_obj_1, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(lv_obj_1, LV_SIZE_CONTENT);
        lv_obj_set_height(lv_obj_1, LV_SIZE_CONTENT);
        lv_obj_set_style_flex_main_place(lv_obj_1, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(lv_obj_1, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(lv_obj_1, &style_id_pill, 0);
        lv_obj_t * patient_id = lv_label_create(lv_obj_1);
        lv_obj_set_name(patient_id, "patient_id");
        lv_label_set_text(patient_id, "ID Pasien: -");

        lv_obj_t * row_0 = row_create(content, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_0, 440);
        lv_obj_set_style_flex_main_place(row_0, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(row_0, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_t * vc_spo2 = result_vital_create(row_0, icon_vital_spo2_sm, COLOR_VITAL_SPO2, "--", "SpO2 %");
        lv_obj_set_name(vc_spo2, "vc_spo2");

        lv_obj_t * vc_hr = result_vital_create(row_0, icon_vital_hr_sm, COLOR_VITAL_HR, "--", "HR bpm");
        lv_obj_set_name(vc_hr, "vc_hr");

        lv_obj_t * vc_rr = result_vital_create(row_0, icon_vital_rr_sm, COLOR_VITAL_RR, "--", "RR /min");
        lv_obj_set_name(vc_rr, "vc_rr");

        lv_obj_t * vc_bp = result_vital_create(row_0, icon_vital_bp_sm, COLOR_VITAL_BP, "--/--", "BP mmHg");
        lv_obj_set_name(vc_bp, "vc_bp");

        lv_obj_t * button_bar_0 = button_bar_create(lv_obj_0, "Monitor", "Reset", "Power", "Menu", icon_monitor, icon_refresh, icon_power, icon_menu, COLOR_DARK_TEXT, COLOR_DARK_TEXT, COLOR_DANGER, COLOR_DARK_TEXT);
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

