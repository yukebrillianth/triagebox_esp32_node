/**
 * @file button_bar_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "button_bar_gen.h"
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

lv_obj_t * button_bar_create(lv_obj_t * parent, const char * label0, const char * label1, const char * label2, const char * label3, const void * icon0, const void * icon1, const void * icon2, const void * icon3, lv_color_t color0, lv_color_t color1, lv_color_t color2, lv_color_t color3)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_button_bar;
    static lv_style_t style_bar_cell;
    static lv_style_t style_bar_cell_pressed;
    static lv_style_t style_cell_icon;
    static lv_style_t style_cell_label;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_button_bar);
        lv_style_init(&style_bar_cell);
        lv_style_init(&style_bar_cell_pressed);
        lv_style_init(&style_cell_icon);
        lv_style_init(&style_cell_label);

        lv_style_set_width(&style_button_bar, 480);
        lv_style_set_height(&style_button_bar, 71);
        lv_style_set_bg_opa(&style_button_bar, (255 * 0 / 100));
        lv_style_set_border_width(&style_button_bar, 0);
        lv_style_set_outline_width(&style_button_bar, 0);
        lv_style_set_shadow_width(&style_button_bar, 0);
        lv_style_set_pad_all(&style_button_bar, 0);
        lv_style_set_pad_gap(&style_button_bar, 0);
        lv_style_set_flex_cross_place(&style_button_bar, LV_FLEX_ALIGN_CENTER);
        lv_style_set_width(&style_bar_cell, 120);
        lv_style_set_height(&style_bar_cell, 71);
        lv_style_set_radius(&style_bar_cell, 0);
        lv_style_set_border_width(&style_bar_cell, 1);
        lv_style_set_border_color(&style_bar_cell, COLOR_DARK_PANEL);
        lv_style_set_border_opa(&style_bar_cell, (255 * 100 / 100));
        lv_style_set_border_side(&style_bar_cell, LV_BORDER_SIDE_FULL);
        lv_style_set_outline_width(&style_bar_cell, 0);
        lv_style_set_outline_opa(&style_bar_cell, (255 * 0 / 100));
        lv_style_set_shadow_width(&style_bar_cell, 0);
        lv_style_set_pad_top(&style_bar_cell, 12);
        lv_style_set_pad_bottom(&style_bar_cell, 12);
        lv_style_set_pad_hor(&style_bar_cell, 0);
        lv_style_set_pad_gap(&style_bar_cell, 4);
        lv_style_set_flex_flow(&style_bar_cell, LV_FLEX_FLOW_COLUMN);
        lv_style_set_flex_cross_place(&style_bar_cell, LV_FLEX_ALIGN_CENTER);
        lv_style_set_flex_main_place(&style_bar_cell, LV_FLEX_ALIGN_START);
        lv_style_set_text_font(&style_bar_cell, font_inter_semi_bold_16);
        lv_style_set_bg_color(&style_bar_cell_pressed, COLOR_TRACK);
        lv_style_set_bg_opa(&style_bar_cell_pressed, (255 * 40 / 100));
        lv_style_set_image_recolor_opa(&style_cell_icon, (255 * 100 / 100));
        lv_style_set_width(&style_cell_label, lv_pct(100));
        lv_style_set_height(&style_cell_label, 19);
        lv_style_set_text_align(&style_cell_label, LV_TEXT_ALIGN_CENTER);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "button_bar_#");
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_button_bar, 0);
        lv_obj_t * cell0 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(cell0, "cell0");
        lv_obj_set_flex_flow(cell0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_text_color(cell0, color0, 0);
        lv_obj_set_flag(cell0, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(cell0, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(cell0, &style_bar_cell, 0);
        lv_obj_add_style(cell0, &style_buttonbar_cell_grad, 0);
        lv_obj_add_style(cell0, &style_bar_cell_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_0 = lv_image_create(cell0);
        lv_image_set_src(lv_image_0, icon0);
        lv_obj_set_width(lv_image_0, 24);
        lv_obj_set_height(lv_image_0, 24);
        lv_obj_set_flag(lv_image_0, LV_OBJ_FLAG_HIDDEN, !icon0);
        lv_obj_set_style_image_recolor(lv_image_0, color0, 0);
        lv_image_set_inner_align(lv_image_0, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_0, &style_cell_icon, 0);

        lv_obj_t * lv_label_0 = lv_label_create(cell0);
        lv_label_set_text(lv_label_0, label0);
        lv_obj_set_flag(lv_label_0, LV_OBJ_FLAG_HIDDEN, !label0);
        lv_obj_add_style(lv_label_0, &style_cell_label, 0);

        lv_obj_t * cell1 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(cell1, "cell1");
        lv_obj_set_flex_flow(cell1, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_text_color(cell1, color1, 0);
        lv_obj_set_flag(cell1, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(cell1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(cell1, &style_bar_cell, 0);
        lv_obj_add_style(cell1, &style_buttonbar_cell_grad, 0);
        lv_obj_add_style(cell1, &style_bar_cell_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_1 = lv_image_create(cell1);
        lv_image_set_src(lv_image_1, icon1);
        lv_obj_set_width(lv_image_1, 24);
        lv_obj_set_height(lv_image_1, 24);
        lv_obj_set_flag(lv_image_1, LV_OBJ_FLAG_HIDDEN, !icon1);
        lv_obj_set_style_image_recolor(lv_image_1, color1, 0);
        lv_image_set_inner_align(lv_image_1, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_1, &style_cell_icon, 0);

        lv_obj_t * lv_label_1 = lv_label_create(cell1);
        lv_label_set_text(lv_label_1, label1);
        lv_obj_set_flag(lv_label_1, LV_OBJ_FLAG_HIDDEN, !label1);
        lv_obj_add_style(lv_label_1, &style_cell_label, 0);

        lv_obj_t * cell2 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(cell2, "cell2");
        lv_obj_set_flex_flow(cell2, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_text_color(cell2, color2, 0);
        lv_obj_set_flag(cell2, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(cell2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(cell2, &style_bar_cell, 0);
        lv_obj_add_style(cell2, &style_buttonbar_cell_grad, 0);
        lv_obj_add_style(cell2, &style_bar_cell_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_2 = lv_image_create(cell2);
        lv_image_set_src(lv_image_2, icon2);
        lv_obj_set_width(lv_image_2, 24);
        lv_obj_set_height(lv_image_2, 24);
        lv_obj_set_flag(lv_image_2, LV_OBJ_FLAG_HIDDEN, !icon2);
        lv_obj_set_style_image_recolor(lv_image_2, color2, 0);
        lv_image_set_inner_align(lv_image_2, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_2, &style_cell_icon, 0);

        lv_obj_t * lv_label_2 = lv_label_create(cell2);
        lv_label_set_text(lv_label_2, label2);
        lv_obj_set_flag(lv_label_2, LV_OBJ_FLAG_HIDDEN, !label2);
        lv_obj_add_style(lv_label_2, &style_cell_label, 0);

        lv_obj_t * cell3 = lv_obj_create(lv_obj_0);
        lv_obj_set_name(cell3, "cell3");
        lv_obj_set_flex_flow(cell3, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_text_color(cell3, color3, 0);
        lv_obj_set_flag(cell3, LV_OBJ_FLAG_CLICKABLE, true);
        lv_obj_set_flag(cell3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_add_style(cell3, &style_bar_cell, 0);
        lv_obj_add_style(cell3, &style_buttonbar_cell_grad, 0);
        lv_obj_add_style(cell3, &style_bar_cell_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_3 = lv_image_create(cell3);
        lv_image_set_src(lv_image_3, icon3);
        lv_obj_set_width(lv_image_3, 24);
        lv_obj_set_height(lv_image_3, 24);
        lv_obj_set_flag(lv_image_3, LV_OBJ_FLAG_HIDDEN, !icon3);
        lv_obj_set_style_image_recolor(lv_image_3, color3, 0);
        lv_image_set_inner_align(lv_image_3, LV_IMAGE_ALIGN_CENTER);
        lv_obj_add_style(lv_image_3, &style_cell_icon, 0);

        lv_obj_t * lv_label_3 = lv_label_create(cell3);
        lv_label_set_text(lv_label_3, label3);
        lv_obj_set_flag(lv_label_3, LV_OBJ_FLAG_HIDDEN, !label3);
        lv_obj_add_style(lv_label_3, &style_cell_label, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

