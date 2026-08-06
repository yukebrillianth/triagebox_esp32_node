/**
 * @file vital_card_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "vital_card_gen.h"
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

lv_obj_t * vital_card_create(lv_obj_t * parent, const void * icon, lv_color_t icon_color, const char * value, const char * unit, const char * label)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_vital_card;
    static lv_style_t style_vital_icon;
    static lv_style_t style_vital_value;
    static lv_style_t style_vital_unit;
    static lv_style_t style_vital_label;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_vital_card);
        lv_style_init(&style_vital_icon);
        lv_style_init(&style_vital_value);
        lv_style_init(&style_vital_unit);
        lv_style_init(&style_vital_label);

        lv_style_set_bg_color(&style_vital_card, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_vital_card, (255 * 100 / 100));
        lv_style_set_radius(&style_vital_card, RADIUS_DEFAULT);
        lv_style_set_pad_ver(&style_vital_card, 12);
        lv_style_set_pad_hor(&style_vital_card, 14);
        lv_style_set_pad_gap(&style_vital_card, SPACE_LG);
        lv_style_set_flex_main_place(&style_vital_card, LV_FLEX_ALIGN_START);
        lv_style_set_flex_cross_place(&style_vital_card, LV_FLEX_ALIGN_CENTER);
        lv_style_set_text_color(&style_vital_card, COLOR_DARK_TEXT);
        lv_style_set_image_recolor_opa(&style_vital_icon, (255 * 100 / 100));
        lv_style_set_width(&style_vital_icon, 28);
        lv_style_set_height(&style_vital_icon, 28);
        lv_style_set_text_font(&style_vital_value, font_inter_bold_20);
        lv_style_set_text_color(&style_vital_value, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_vital_unit, font_body);
        lv_style_set_text_color(&style_vital_unit, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_vital_label, font_inter_regular_12);
        lv_style_set_text_color(&style_vital_label, COLOR_TEXT_SECONDARY);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "vital_card_#");
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, lv_pct(100));

        lv_obj_add_style(lv_obj_0, &style_vital_card, 0);
        lv_obj_t * lv_image_0 = lv_image_create(lv_obj_0);
        lv_image_set_src(lv_image_0, icon);
        lv_obj_set_style_image_recolor(lv_image_0, icon_color, 0);
        lv_obj_add_style(lv_image_0, &style_vital_icon, 0);

        lv_obj_t * column_0 = column_create(lv_obj_0, 0, 2, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(column_0, 0);
        lv_obj_set_height(column_0, lv_pct(100));
        lv_obj_set_flex_grow(column_0, 1);
        lv_obj_t * lv_label_0 = lv_label_create(column_0);
        lv_label_set_text(lv_label_0, label);
        lv_obj_add_style(lv_label_0, &style_vital_label, 0);

        lv_obj_t * row_0 = row_create(column_0, 0, SPACE_XS, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_0, LV_SIZE_CONTENT);
        lv_obj_t * lv_label_1 = lv_label_create(row_0);
        lv_label_set_text(lv_label_1, value);
        lv_obj_add_style(lv_label_1, &style_vital_value, 0);

        lv_obj_t * lv_label_2 = lv_label_create(row_0);
        lv_label_set_text(lv_label_2, unit);
        lv_obj_set_flag(lv_label_2, LV_OBJ_FLAG_HIDDEN, !unit);
        lv_obj_add_style(lv_label_2, &style_vital_unit, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

