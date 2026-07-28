#ifndef UI_LOGIC_UI_INPUT_H
#define UI_LOGIC_UI_INPUT_H

#include "lvgl.h"

/*
 * Button 0/1/2/3 -> LV_KEY_PREV/NEXT/ENTER/ESC.
 * Planned simulator producer: SDL keys 1/2/3/4 -> mock buttons 0/1/2/3.
 */
lv_indev_t *ui_input_keypad_init(lv_display_t *display);

/* Caller adds list objects, then assigns the group with lv_indev_set_group(). */
lv_group_t *ui_input_create_group(void);

#endif /* UI_LOGIC_UI_INPUT_H */
