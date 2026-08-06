/**
 * @file status_bar_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "status_bar_gen.h"
#include "../../ui.h"

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

lv_obj_t * status_bar_create(lv_obj_t * parent, const void * battery_icon, const char * battery_text, const char * link_text, const char * clock_text)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_status_bar;
    static lv_style_t style_status_battery_icon;
    static lv_style_t style_status_accent_icon;
    static lv_style_t style_status_clock;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_status_bar);
        lv_style_init(&style_status_battery_icon);
        lv_style_init(&style_status_accent_icon);
        lv_style_init(&style_status_clock);

        lv_style_set_width(&style_status_bar, 480);
        lv_style_set_height(&style_status_bar, 48);
        lv_style_set_bg_color(&style_status_bar, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_status_bar, (255 * 100 / 100));
        lv_style_set_pad_hor(&style_status_bar, 20);
        lv_style_set_pad_ver(&style_status_bar, 12);
        lv_style_set_flex_cross_place(&style_status_bar, LV_FLEX_ALIGN_CENTER);
        lv_style_set_text_color(&style_status_bar, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_status_bar, font_inter_semi_bold_16);
        lv_style_set_width(&style_status_battery_icon, 24);
        lv_style_set_height(&style_status_battery_icon, 24);
        lv_style_set_width(&style_status_accent_icon, 24);
        lv_style_set_height(&style_status_accent_icon, 24);
        lv_style_set_image_recolor(&style_status_accent_icon, COLOR_ACCENT);
        lv_style_set_image_recolor_opa(&style_status_accent_icon, (255 * 100 / 100));
        lv_style_set_text_font(&style_status_clock, font_inter_regular_16);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "status_bar_#");
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_flex_main_place(lv_obj_0, LV_FLEX_ALIGN_SPACE_BETWEEN, 0);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_status_bar, 0);
        lv_obj_t * row_0 = row_create(lv_obj_0, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_t * row_1 = row_create(row_0, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_t * sb_battery = lv_image_create(row_1);
        lv_obj_set_name(sb_battery, "sb_battery");
        lv_image_set_src(sb_battery, battery_icon);
        lv_obj_set_width(sb_battery, 24);
        lv_obj_set_height(sb_battery, 24);
        lv_image_set_inner_align(sb_battery, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(sb_battery, &style_status_battery_icon, 0);

        lv_obj_t * sb_battery_text = lv_label_create(row_1);
        lv_obj_set_name(sb_battery_text, "sb_battery_text");
        lv_label_set_text(sb_battery_text, battery_text);

        lv_obj_t * row_2 = row_create(row_0, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_t * lv_image_0 = lv_image_create(row_2);
        lv_image_set_src(lv_image_0, icon_signal);
        lv_obj_set_width(lv_image_0, 24);
        lv_obj_set_height(lv_image_0, 24);
        lv_image_set_inner_align(lv_image_0, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_0, &style_status_accent_icon, 0);

        lv_obj_t * sb_link_text = lv_label_create(row_2);
        lv_obj_set_name(sb_link_text, "sb_link_text");
        lv_label_set_text(sb_link_text, link_text);

        lv_obj_t * row_3 = row_create(lv_obj_0, 0, 0, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_t * sb_clock = lv_label_create(row_3);
        lv_obj_set_name(sb_clock, "sb_clock");
        lv_label_set_text(sb_clock, clock_text);
        lv_obj_add_style(sb_clock, &style_status_clock, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

