/**
 * @file scanning_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "scanning_gen.h"
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

lv_obj_t * scanning_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_scan_root;
    static lv_style_t style_scan_content;
    static lv_style_t style_scan_icon;
    static lv_style_t style_scan_title;
    static lv_style_t style_scan_sub;
    static lv_style_t style_scan_dot;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_scan_root);
        lv_style_init(&style_scan_content);
        lv_style_init(&style_scan_icon);
        lv_style_init(&style_scan_title);
        lv_style_init(&style_scan_sub);
        lv_style_init(&style_scan_dot);

        lv_style_set_bg_color(&style_scan_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_scan_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_scan_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_scan_root, 0);
        lv_style_set_pad_gap(&style_scan_root, 0);
        lv_style_set_pad_hor(&style_scan_content, 40);
        lv_style_set_pad_ver(&style_scan_content, 0);
        lv_style_set_image_recolor(&style_scan_icon, COLOR_ACCENT);
        lv_style_set_image_recolor_opa(&style_scan_icon, (255 * 100 / 100));
        lv_style_set_width(&style_scan_icon, 80);
        lv_style_set_height(&style_scan_icon, 80);
        lv_style_set_text_color(&style_scan_title, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_scan_title, font_inter_semi_bold_24);
        lv_style_set_text_align(&style_scan_title, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_color(&style_scan_sub, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_scan_sub, font_inter_regular_14);
        lv_style_set_text_align(&style_scan_sub, LV_TEXT_ALIGN_CENTER);
        lv_style_set_bg_color(&style_scan_dot, COLOR_ACCENT);
        lv_style_set_bg_opa(&style_scan_dot, (255 * 100 / 100));
        lv_style_set_radius(&style_scan_dot, lv_pct(100));
        lv_style_set_width(&style_scan_dot, 12);
        lv_style_set_height(&style_scan_dot, 12);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (scanning == NULL) scanning = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = scanning;
        lv_obj_set_name_static(lv_obj_0, "scanning_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_scan_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 0, 24, 1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_add_style(content, &style_scan_content, 0);
        lv_obj_t * lv_image_0 = lv_image_create(content);
        lv_image_set_src(lv_image_0, icon_search);
        lv_obj_set_width(lv_image_0, 80);
        lv_obj_set_height(lv_image_0, 80);
        lv_image_set_inner_align(lv_image_0, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_0, &style_scan_icon, 0);

        lv_obj_t * column_0 = column_create(content, 0, 8, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(column_0, lv_pct(100));
        lv_obj_t * lv_label_0 = lv_label_create(column_0);
        lv_label_set_text(lv_label_0, "Memindai RFID...");
        lv_obj_set_width(lv_label_0, lv_pct(100));
        lv_obj_add_style(lv_label_0, &style_scan_title, 0);

        lv_obj_t * lv_label_1 = lv_label_create(column_0);
        lv_label_set_text(lv_label_1, "Dekatkan gelang pasien ke sensor");
        lv_obj_set_width(lv_label_1, lv_pct(100));
        lv_obj_add_style(lv_label_1, &style_scan_sub, 0);

        lv_obj_t * row_0 = row_create(content, 0, 8, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_t * lv_obj_1 = lv_obj_create(row_0);
        lv_obj_add_style(lv_obj_1, &style_scan_dot, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(row_0);
        lv_obj_add_style(lv_obj_2, &style_scan_dot, 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(row_0);
        lv_obj_add_style(lv_obj_3, &style_scan_dot, 0);

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

