/**
 * @file ui.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "ui.h"
#include "logic/ui_bindings.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ui_init(const char * asset_path)
{
    LV_LOG("Initializing custom C code using LVGL v%d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    ui_init_gen(asset_path);
    ui_bindings_init();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/