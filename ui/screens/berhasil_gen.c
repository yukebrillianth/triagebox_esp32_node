/**
 * @file berhasil_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "berhasil_gen.h"
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

lv_obj_t * berhasil_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_ok_root;
    static lv_style_t style_ok_content;
    static lv_style_t style_ok_circle;
    static lv_style_t style_ok_icon;
    static lv_style_t style_ok_title;
    static lv_style_t style_ok_id_card;
    static lv_style_t style_ok_id_label;
    static lv_style_t style_ok_id_value;
    static lv_style_t style_ok_hint;
    static lv_style_t style_ok_hint_text;
    static lv_style_t style_ok_hint_accent;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_ok_root);
        lv_style_init(&style_ok_content);
        lv_style_init(&style_ok_circle);
        lv_style_init(&style_ok_icon);
        lv_style_init(&style_ok_title);
        lv_style_init(&style_ok_id_card);
        lv_style_init(&style_ok_id_label);
        lv_style_init(&style_ok_id_value);
        lv_style_init(&style_ok_hint);
        lv_style_init(&style_ok_hint_text);
        lv_style_init(&style_ok_hint_accent);

        lv_style_set_bg_color(&style_ok_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_ok_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_ok_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_ok_root, 0);
        lv_style_set_pad_gap(&style_ok_root, 0);
        lv_style_set_pad_hor(&style_ok_content, 40);
        lv_style_set_pad_ver(&style_ok_content, 0);
        lv_style_set_bg_color(&style_ok_circle, COLOR_ACCENT);
        lv_style_set_bg_opa(&style_ok_circle, (255 * 100 / 100));
        lv_style_set_radius(&style_ok_circle, lv_pct(100));
        lv_style_set_width(&style_ok_circle, 100);
        lv_style_set_height(&style_ok_circle, 100);
        lv_style_set_flex_main_place(&style_ok_circle, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_cross_place(&style_ok_circle, LV_FLEX_ALIGN_CENTER);
        lv_style_set_pad_all(&style_ok_circle, 0);
        lv_style_set_border_width(&style_ok_circle, 0);
        lv_style_set_outline_width(&style_ok_circle, 0);
        lv_style_set_shadow_width(&style_ok_circle, 0);
        lv_style_set_image_recolor(&style_ok_icon, COLOR_ACCENT_TEXT);
        lv_style_set_image_recolor_opa(&style_ok_icon, (255 * 100 / 100));
        lv_style_set_width(&style_ok_icon, 48);
        lv_style_set_height(&style_ok_icon, 48);
        lv_style_set_text_color(&style_ok_title, COLOR_ACCENT);
        lv_style_set_text_font(&style_ok_title, font_inter_semi_bold_24);
        lv_style_set_text_align(&style_ok_title, LV_TEXT_ALIGN_CENTER);
        lv_style_set_bg_color(&style_ok_id_card, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_ok_id_card, (255 * 100 / 100));
        lv_style_set_radius(&style_ok_id_card, RADIUS_DEFAULT);
        lv_style_set_pad_all(&style_ok_id_card, 16);
        lv_style_set_border_width(&style_ok_id_card, 0);
        lv_style_set_outline_width(&style_ok_id_card, 0);
        lv_style_set_shadow_width(&style_ok_id_card, 0);
        lv_style_set_width(&style_ok_id_card, 380);
        lv_style_set_text_color(&style_ok_id_label, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_ok_id_label, font_inter_regular_12);
        lv_style_set_text_align(&style_ok_id_label, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_color(&style_ok_id_value, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_ok_id_value, font_inter_semi_bold_20);
        lv_style_set_text_align(&style_ok_id_value, LV_TEXT_ALIGN_CENTER);
        lv_style_set_bg_color(&style_ok_hint, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_ok_hint, (255 * 100 / 100));
        lv_style_set_radius(&style_ok_hint, RADIUS_DEFAULT);
        lv_style_set_pad_all(&style_ok_hint, 12);
        lv_style_set_border_width(&style_ok_hint, 0);
        lv_style_set_outline_width(&style_ok_hint, 0);
        lv_style_set_shadow_width(&style_ok_hint, 0);
        lv_style_set_width(&style_ok_hint, 380);
        lv_style_set_text_color(&style_ok_hint_text, COLOR_TEXT_ON_CARD);
        lv_style_set_text_font(&style_ok_hint_text, font_inter_regular_13);
        lv_style_set_text_align(&style_ok_hint_text, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_color(&style_ok_hint_accent, COLOR_ACCENT);
        lv_style_set_text_font(&style_ok_hint_accent, font_inter_semi_bold_13);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (berhasil == NULL) berhasil = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = berhasil;
        lv_obj_set_name_static(lv_obj_0, "berhasil_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_ok_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 0, 16, 1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_add_style(content, &style_ok_content, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(content);
        lv_obj_set_flex_flow(lv_obj_1, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(lv_obj_1, 100);
        lv_obj_set_height(lv_obj_1, 100);
        lv_obj_add_style(lv_obj_1, &style_ok_circle, 0);
        lv_obj_t * lv_image_0 = lv_image_create(lv_obj_1);
        lv_image_set_src(lv_image_0, icon_check);
        lv_obj_set_width(lv_image_0, 48);
        lv_obj_set_height(lv_image_0, 48);
        lv_image_set_inner_align(lv_image_0, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_0, &style_ok_icon, 0);

        lv_obj_t * lv_label_0 = lv_label_create(content);
        lv_label_set_text(lv_label_0, "Scan Berhasil!");
        lv_obj_set_width(lv_label_0, lv_pct(100));
        lv_obj_add_style(lv_label_0, &style_ok_title, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(content);
        lv_obj_set_flex_flow(lv_obj_2, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_flex_main_place(lv_obj_2, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(lv_obj_2, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_width(lv_obj_2, 380);
        lv_obj_add_style(lv_obj_2, &style_ok_id_card, 0);
        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_1, "ID Pasien:");
        lv_obj_set_width(lv_label_1, lv_pct(100));
        lv_obj_add_style(lv_label_1, &style_ok_id_label, 0);

        lv_obj_t * patient_id = lv_label_create(lv_obj_2);
        lv_obj_set_name(patient_id, "patient_id");
        lv_label_set_text(patient_id, "-");
        lv_obj_set_width(patient_id, lv_pct(100));
        lv_obj_add_style(patient_id, &style_ok_id_value, 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(content);
        lv_obj_set_flex_flow(lv_obj_3, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_flex_main_place(lv_obj_3, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(lv_obj_3, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_width(lv_obj_3, 380);
        lv_obj_set_height(lv_obj_3, LV_SIZE_CONTENT);
        lv_obj_add_style(lv_obj_3, &style_ok_hint, 0);
        lv_obj_t * lv_spangroup_0 = lv_spangroup_create(lv_obj_3);
        lv_obj_set_width(lv_spangroup_0, lv_pct(100));
        lv_obj_set_style_text_align(lv_spangroup_0, LV_TEXT_ALIGN_CENTER, 0);
        lv_span_t * lv_spangroup_span_0 = lv_spangroup_add_span(lv_spangroup_0);
        lv_spangroup_set_span_text(lv_spangroup_0, lv_spangroup_span_0, "Tekan ");
        lv_spangroup_set_span_style(lv_spangroup_0, lv_spangroup_span_0, &style_ok_hint_text);
        lv_span_t * lv_spangroup_span_1 = lv_spangroup_add_span(lv_spangroup_0);
        lv_spangroup_set_span_text(lv_spangroup_0, lv_spangroup_span_1, "START");
        lv_spangroup_set_span_style(lv_spangroup_0, lv_spangroup_span_1, &style_ok_hint_accent);
        lv_span_t * lv_spangroup_span_2 = lv_spangroup_add_span(lv_spangroup_0);
        lv_spangroup_set_span_text(lv_spangroup_0, lv_spangroup_span_2, " untuk mulai pengukuran");
        lv_spangroup_set_span_style(lv_spangroup_0, lv_spangroup_span_2, &style_ok_hint_text);

        lv_obj_t * button_bar_0 = button_bar_create(lv_obj_0, "Start", "Restart", "Power", "Menu", icon_play, icon_refresh, icon_power, icon_menu, COLOR_DARK_TEXT, COLOR_DARK_TEXT, COLOR_DANGER, COLOR_DARK_TEXT);
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

