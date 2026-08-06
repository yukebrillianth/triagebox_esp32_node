/**
 * @file mengukur_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "mengukur_gen.h"
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

lv_obj_t * mengukur_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_measure_root;
    static lv_style_t style_measure_content;
    static lv_style_t style_measure_title;
    static lv_style_t style_measure_sub;
    static lv_style_t style_progress_track;
    static lv_style_t style_progress_indicator;
    static lv_style_t style_progress_pct;
    static lv_style_t style_measure_vital;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_measure_root);
        lv_style_init(&style_measure_content);
        lv_style_init(&style_measure_title);
        lv_style_init(&style_measure_sub);
        lv_style_init(&style_progress_track);
        lv_style_init(&style_progress_indicator);
        lv_style_init(&style_progress_pct);
        lv_style_init(&style_measure_vital);

        lv_style_set_bg_color(&style_measure_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_measure_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_measure_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_measure_root, 0);
        lv_style_set_pad_gap(&style_measure_root, 0);
        lv_style_set_pad_hor(&style_measure_content, 32);
        lv_style_set_pad_ver(&style_measure_content, 0);
        lv_style_set_text_color(&style_measure_title, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_measure_title, font_inter_semi_bold_20);
        lv_style_set_text_align(&style_measure_title, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_color(&style_measure_sub, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_measure_sub, font_inter_regular_12);
        lv_style_set_text_align(&style_measure_sub, LV_TEXT_ALIGN_CENTER);
        lv_style_set_width(&style_progress_track, 400);
        lv_style_set_height(&style_progress_track, 8);
        lv_style_set_bg_color(&style_progress_track, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_progress_track, (255 * 100 / 100));
        lv_style_set_radius(&style_progress_track, 100);
        lv_style_set_border_width(&style_progress_track, 0);
        lv_style_set_outline_width(&style_progress_track, 0);
        lv_style_set_shadow_width(&style_progress_track, 0);
        lv_style_set_bg_color(&style_progress_indicator, COLOR_ACCENT);
        lv_style_set_bg_opa(&style_progress_indicator, (255 * 100 / 100));
        lv_style_set_radius(&style_progress_indicator, 100);
        lv_style_set_text_color(&style_progress_pct, COLOR_ACCENT_TEXT);
        lv_style_set_text_font(&style_progress_pct, font_inter_regular_14);
        lv_style_set_text_align(&style_progress_pct, LV_TEXT_ALIGN_CENTER);
        lv_style_set_width(&style_measure_vital, 200);
        lv_style_set_height(&style_measure_vital, 88);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (mengukur == NULL) mengukur = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = mengukur;
        lv_obj_set_name_static(lv_obj_0, "mengukur_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_measure_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 0, 8, 1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_add_style(content, &style_measure_content, 0);
        lv_obj_t * column_0 = column_create(content, 0, 4, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(column_0, lv_pct(100));
        lv_obj_t * lv_label_0 = lv_label_create(column_0);
        lv_label_set_text(lv_label_0, "Mengukur...");
        lv_obj_set_width(lv_label_0, lv_pct(100));
        lv_obj_add_style(lv_label_0, &style_measure_title, 0);

        lv_obj_t * lv_label_1 = lv_label_create(column_0);
        lv_label_set_text(lv_label_1, "Mohon tunggu, jangan gerakkan sensor");
        lv_obj_set_width(lv_label_1, lv_pct(100));
        lv_obj_add_style(lv_label_1, &style_measure_sub, 0);

        lv_obj_t * column_1 = column_create(content, 0, 12, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(column_1, lv_pct(100));
        lv_obj_t * measure_progress = lv_bar_create(column_1);
        lv_obj_set_name(measure_progress, "measure_progress");
        lv_bar_set_value(measure_progress, 0, false);
        lv_bar_set_min_value(measure_progress, 0);
        lv_bar_set_max_value(measure_progress, 100);
        lv_obj_set_width(measure_progress, 400);
        lv_obj_set_height(measure_progress, 8);
        lv_obj_add_style(measure_progress, &style_progress_track, 0);
        lv_obj_add_style(measure_progress, &style_progress_indicator, LV_PART_INDICATOR);

        lv_obj_t * measure_pct = lv_label_create(column_1);
        lv_obj_set_name(measure_pct, "measure_pct");
        lv_label_set_text(measure_pct, "0%");
        lv_obj_set_width(measure_pct, lv_pct(100));
        lv_obj_add_style(measure_pct, &style_progress_pct, 0);

        lv_obj_t * row_0 = row_create(content, 0, 12, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_0, lv_pct(100));
        lv_obj_set_style_margin_bottom(row_0, 4, 0);
        lv_obj_set_style_margin_top(row_0, 12, 0);
        lv_obj_t * vc_spo2 = vital_card_create(row_0, icon_oxygen, COLOR_VITAL_SPO2, "--", "%", "SpO2");
        lv_obj_set_name(vc_spo2, "vc_spo2");
        lv_obj_set_width(vc_spo2, 200);
        lv_obj_set_height(vc_spo2, 88);
        lv_obj_add_style(vc_spo2, &style_measure_vital, 0);

        lv_obj_t * vc_rr = vital_card_create(row_0, icon_respiratory, COLOR_VITAL_RR, "--", "/min", "Laju Pernapasan");
        lv_obj_set_name(vc_rr, "vc_rr");
        lv_obj_set_width(vc_rr, 200);
        lv_obj_set_height(vc_rr, 88);
        lv_obj_add_style(vc_rr, &style_measure_vital, 0);

        lv_obj_t * row_1 = row_create(content, 0, 12, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_1, lv_pct(100));
        lv_obj_t * vc_hr = vital_card_create(row_1, icon_heart_pulse, COLOR_VITAL_HR, "--", "bpm", "Detak Jantung");
        lv_obj_set_name(vc_hr, "vc_hr");
        lv_obj_set_width(vc_hr, 200);
        lv_obj_set_height(vc_hr, 88);
        lv_obj_add_style(vc_hr, &style_measure_vital, 0);

        lv_obj_t * vc_bp = vital_card_create(row_1, icon_blood_pressure, COLOR_VITAL_BP, "--/--", "mmHg", "Tekanan Darah");
        lv_obj_set_name(vc_bp, "vc_bp");
        lv_obj_set_width(vc_bp, 200);
        lv_obj_set_height(vc_bp, 88);
        lv_obj_add_style(vc_bp, &style_measure_vital, 0);

        lv_obj_t * button_bar_0 = button_bar_create(lv_obj_0, "Abort", "", "Power", "Menu", icon_close, NULL, icon_power, icon_menu, COLOR_DANGER, COLOR_DARK_TEXT, COLOR_DANGER, COLOR_DARK_TEXT);
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

