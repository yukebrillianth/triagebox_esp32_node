/**
 * @file result_vital_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "result_vital_gen.h"
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

lv_obj_t * result_vital_create(lv_obj_t * parent, const void * icon, lv_color_t icon_color, const char * value, const char * caption)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_rv_card;
    static lv_style_t style_rv_icon;
    static lv_style_t style_rv_value;
    static lv_style_t style_rv_label;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_rv_card);
        lv_style_init(&style_rv_icon);
        lv_style_init(&style_rv_value);
        lv_style_init(&style_rv_label);

        lv_style_set_bg_color(&style_rv_card, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_rv_card, (255 * 100 / 100));
        lv_style_set_radius(&style_rv_card, RADIUS_DEFAULT);
        lv_style_set_pad_ver(&style_rv_card, 10);
        lv_style_set_pad_hor(&style_rv_card, 8);
        lv_style_set_pad_gap(&style_rv_card, 4);
        lv_style_set_flex_main_place(&style_rv_card, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_cross_place(&style_rv_card, LV_FLEX_ALIGN_CENTER);
        lv_style_set_border_width(&style_rv_card, 0);
        lv_style_set_outline_width(&style_rv_card, 0);
        lv_style_set_shadow_width(&style_rv_card, 0);
        lv_style_set_text_color(&style_rv_card, COLOR_DARK_TEXT);
        lv_style_set_image_recolor_opa(&style_rv_icon, (255 * 100 / 100));
        lv_style_set_width(&style_rv_icon, 20);
        lv_style_set_height(&style_rv_icon, 20);
        lv_style_set_text_font(&style_rv_value, font_inter_bold_20);
        lv_style_set_text_color(&style_rv_value, COLOR_DARK_TEXT);
        lv_style_set_text_align(&style_rv_value, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_font(&style_rv_label, font_inter_regular_12);
        lv_style_set_text_color(&style_rv_label, COLOR_TEXT_SECONDARY);
        lv_style_set_text_align(&style_rv_label, LV_TEXT_ALIGN_CENTER);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "result_vital_#");
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(lv_obj_0, 104);
        lv_obj_set_height(lv_obj_0, 89);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_rv_card, 0);
        lv_obj_t * lv_image_0 = lv_image_create(lv_obj_0);
        lv_image_set_src(lv_image_0, icon);
        lv_obj_set_width(lv_image_0, 20);
        lv_obj_set_height(lv_image_0, 20);
        lv_obj_set_style_image_recolor(lv_image_0, icon_color, 0);
        lv_obj_add_style(lv_image_0, &style_rv_icon, 0);

        lv_obj_t * lbl_value = lv_label_create(lv_obj_0);
        lv_obj_set_name(lbl_value, "lbl_value");
        lv_label_set_text(lbl_value, value);
        lv_obj_set_width(lbl_value, lv_pct(100));
        lv_obj_add_style(lbl_value, &style_rv_value, 0);

        lv_obj_t * lbl_caption = lv_label_create(lv_obj_0);
        lv_obj_set_name(lbl_caption, "lbl_caption");
        lv_label_set_text(lbl_caption, caption);
        lv_obj_set_width(lbl_caption, lv_pct(100));
        lv_obj_add_style(lbl_caption, &style_rv_label, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

