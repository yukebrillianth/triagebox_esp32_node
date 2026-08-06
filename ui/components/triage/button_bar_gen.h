/**
 * @file button_bar_gen.h
 */

#ifndef BUTTON_BAR_H
#define BUTTON_BAR_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
    #include "lvgl_private.h"
#else
    #include "lvgl/lvgl.h"
    #include "lvgl/lvgl_private.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

lv_obj_t * button_bar_create(lv_obj_t * parent, const char * label0, const char * label1, const char * label2, const char * label3, const void * icon0, const void * icon1, const void * icon2, const void * icon3, lv_color_t color0, lv_color_t color1, lv_color_t color2, lv_color_t color3);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*BUTTON_BAR_H*/