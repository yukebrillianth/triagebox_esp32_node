/**
 * @file test_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "test_gen.h"
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

lv_obj_t * test_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_test_root;
    static lv_style_t style_test_title;
    static lv_style_t style_test_hint;
    static lv_style_t style_test_row;
    static lv_style_t style_test_label;
    static lv_style_t style_test_value;
    static lv_style_t style_test_dot;
    static lv_style_t style_test_alive;

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&style_test_root);
        lv_style_init(&style_test_title);
        lv_style_init(&style_test_hint);
        lv_style_init(&style_test_row);
        lv_style_init(&style_test_label);
        lv_style_init(&style_test_value);
        lv_style_init(&style_test_dot);
        lv_style_init(&style_test_alive);

        lv_style_set_bg_color(&style_test_root, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_test_root, (255 * 100 / 100));
        lv_style_set_text_color(&style_test_root, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_test_root, 0);
        lv_style_set_pad_gap(&style_test_root, 0);
        lv_style_set_text_color(&style_test_title, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_test_title, font_inter_semi_bold_18);
        lv_style_set_text_align(&style_test_title, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_color(&style_test_hint, COLOR_TEXT_SECONDARY);
        lv_style_set_text_font(&style_test_hint, font_inter_regular_12);
        lv_style_set_width(&style_test_row, lv_pct(100));
        lv_style_set_bg_color(&style_test_row, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_test_row, (255 * 100 / 100));
        lv_style_set_radius(&style_test_row, RADIUS_DEFAULT);
        lv_style_set_border_width(&style_test_row, 0);
        lv_style_set_outline_width(&style_test_row, 0);
        lv_style_set_shadow_width(&style_test_row, 0);
        lv_style_set_text_color(&style_test_label, COLOR_TEXT_ON_CARD);
        lv_style_set_text_font(&style_test_label, font_inter_regular_14);
        lv_style_set_text_color(&style_test_value, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_test_value, font_inter_semi_bold_20);
        lv_style_set_bg_color(&style_test_dot, COLOR_STATUS_OK);
        lv_style_set_bg_opa(&style_test_dot, (255 * 100 / 100));
        lv_style_set_radius(&style_test_dot, lv_pct(100));
        lv_style_set_width(&style_test_dot, 12);
        lv_style_set_height(&style_test_dot, 12);
        lv_style_set_text_color(&style_test_alive, COLOR_STATUS_OK);
        lv_style_set_text_font(&style_test_alive, font_inter_semi_bold_16);

        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (test == NULL) test = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = test;
        lv_obj_set_name_static(lv_obj_0, "test_#");
        lv_obj_set_width(lv_obj_0, 480);
        lv_obj_set_height(lv_obj_0, 480);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flag(lv_obj_0, LV_OBJ_FLAG_SCROLLABLE, false);

        lv_obj_add_style(lv_obj_0, &style_test_root, 0);
        lv_obj_t * status_bar_0 = status_bar_create(lv_obj_0, battery_full, "80%", "Connected", "--:--");
        lv_obj_set_width(status_bar_0, 480);
        lv_obj_set_height(status_bar_0, 48);

        lv_obj_t * content = column_create(lv_obj_0, 0, 10, 0, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_name(content, "content");
        lv_obj_set_width(content, lv_pct(100));
        lv_obj_set_height(content, 361);
        lv_obj_set_flag(content, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_style_pad_hor(content, 20, 0);
        lv_obj_set_style_pad_ver(content, 12, 0);

        lv_obj_t * lv_label_0 = lv_label_create(content);
        lv_label_set_text(lv_label_0, "Uji Jangkauan");
        lv_obj_set_width(lv_label_0, lv_pct(100));
        lv_obj_set_style_margin_bottom(lv_label_0, 4, 0);
        lv_obj_add_style(lv_label_0, &style_test_title, 0);

        lv_obj_t * row_0 = row_create(content, 16, 8, 0, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(row_0, lv_pct(100));
        lv_obj_set_height(row_0, LV_SIZE_CONTENT);
        lv_obj_add_style(row_0, &style_test_row, 0);

        lv_obj_t * row_0_left = row_create(row_0, 0, 8, 0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row_0_left, LV_SIZE_CONTENT);
        lv_obj_set_height(row_0_left, LV_SIZE_CONTENT);
        lv_obj_set_style_flex_cross_place(row_0_left, LV_FLEX_ALIGN_CENTER, 0);

        lv_obj_t * test_alive_dot = lv_obj_create(row_0_left);
        lv_obj_set_name(test_alive_dot, "test_alive_dot");
        lv_obj_add_style(test_alive_dot, &style_test_dot, 0);

        lv_obj_t * test_alive = lv_label_create(row_0_left);
        lv_obj_set_name(test_alive, "test_alive");
        lv_label_set_text(test_alive, "LINK --");
        lv_obj_add_style(test_alive, &style_test_alive, 0);

        lv_obj_t * test_rssi = lv_label_create(row_0);
        lv_obj_set_name(test_rssi, "test_rssi");
        lv_label_set_text(test_rssi, "-- dBm");
        lv_obj_add_style(test_rssi, &style_test_value, 0);

        lv_obj_t * row_1 = row_create(content, 16, 8, 0, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(row_1, lv_pct(100));
        lv_obj_set_height(row_1, LV_SIZE_CONTENT);
        lv_obj_add_style(row_1, &style_test_row, 0);

        lv_obj_t * lv_label_1 = lv_label_create(row_1);
        lv_label_set_text(lv_label_1, "Baterai");
        lv_obj_add_style(lv_label_1, &style_test_label, 0);

        lv_obj_t * test_battery = lv_label_create(row_1);
        lv_obj_set_name(test_battery, "test_battery");
        lv_label_set_text(test_battery, "--% / --,-- V");
        lv_obj_add_style(test_battery, &style_test_value, 0);

        lv_obj_t * row_2 = row_create(content, 16, 8, 0, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(row_2, lv_pct(100));
        lv_obj_set_height(row_2, LV_SIZE_CONTENT);
        lv_obj_add_style(row_2, &style_test_row, 0);

        lv_obj_t * lv_label_2 = lv_label_create(row_2);
        lv_label_set_text(lv_label_2, "Poll STM32 ok");
        lv_obj_add_style(lv_label_2, &style_test_label, 0);

        lv_obj_t * test_polls_ok = lv_label_create(row_2);
        lv_obj_set_name(test_polls_ok, "test_polls_ok");
        lv_label_set_text(test_polls_ok, "--");
        lv_obj_add_style(test_polls_ok, &style_test_value, 0);

        lv_obj_t * row_3 = row_create(content, 16, 8, 0, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_width(row_3, lv_pct(100));
        lv_obj_set_height(row_3, LV_SIZE_CONTENT);
        lv_obj_add_style(row_3, &style_test_row, 0);

        lv_obj_t * lv_label_3 = lv_label_create(row_3);
        lv_label_set_text(lv_label_3, "Poll STM32 gagal");
        lv_obj_add_style(lv_label_3, &style_test_label, 0);

        lv_obj_t * test_polls_failed = lv_label_create(row_3);
        lv_obj_set_name(test_polls_failed, "test_polls_failed");
        lv_label_set_text(test_polls_failed, "--");
        lv_obj_add_style(test_polls_failed, &style_test_value, 0);

        lv_obj_t * lv_label_hint = lv_label_create(content);
        lv_label_set_text(lv_label_hint, "RSSI = poll terakhir dari stasiun. Poll = link I2C ke STM32, bukan paket LoRa.");
        lv_label_set_long_mode(lv_label_hint, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_width(lv_label_hint, lv_pct(100));
        lv_obj_set_style_margin_top(lv_label_hint, 2, 0);
        lv_obj_add_style(lv_label_hint, &style_test_hint, 0);

        lv_obj_t * button_bar_0 = button_bar_create(lv_obj_0, "Back", "", "Power", "Menu", icon_arrow_left, NULL, icon_power, icon_menu, COLOR_DARK_TEXT, COLOR_DARK_TEXT, COLOR_DANGER, COLOR_DARK_TEXT);
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
