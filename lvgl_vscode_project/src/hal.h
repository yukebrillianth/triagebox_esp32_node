/**
 * @file hal.h
 *
 * Hardware Abstraction Layer for the LVGL PC simulator.
 * Creates the display and input devices using SDL (Linux/macOS) or LVGL's
 * built-in Windows driver.
 */

#ifndef HAL_H
#define HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * Create the simulator display and input devices.
 * @param hor_res   horizontal resolution in pixels
 * @param ver_res   vertical resolution in pixels
 * @return          the created display
 */
lv_display_t * hal_init(int32_t hor_res, int32_t ver_res);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*HAL_H*/
