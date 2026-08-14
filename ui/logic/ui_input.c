#include "lvgl.h"

#include "ui_input.h"
#include "ui_action.h"
#include "ui_mock.h"
#include "ui_types.h"

/* Button 0/1/2/3 -> LV_KEY_PREV/NEXT/ENTER/ESC. */
static const uint32_t s_button_keys[] = {
    LV_KEY_PREV,
    LV_KEY_NEXT,
    LV_KEY_ENTER,
    LV_KEY_ESC,
};

static uint32_t s_last_key = LV_KEY_PREV;

static void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    btn_event_t event;

    (void)indev;
    data->key = s_last_key;
    data->state = LV_INDEV_STATE_RELEASED;

    if(ui_mock_pop_button(&event) && event.index < 4U) {
        s_last_key = s_button_keys[event.index];
        data->key = s_last_key;
        data->state = event.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

        /*
         * Dispatch here as well as handing the key to LVGL. The bottom
         * ButtonBar is a fixed action bar, not a focus group (AGENTS.md), so no
         * widget is focused and LVGL alone would drop the key -- physical
         * buttons did nothing at all before this. Same ui_action() the touch
         * handler uses, so both paths stay identical.
         */
        if(event.pressed) ui_action_on_key(s_last_key);
    }
}

lv_indev_t *ui_input_keypad_init(lv_display_t *display)
{
    lv_indev_t *indev = lv_indev_create();

    if(indev == NULL) return NULL;

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keypad_read_cb);
    if(display != NULL) lv_indev_set_display(indev, display);

    return indev;
}

lv_group_t *ui_input_create_group(void)
{
    return lv_group_create();
}
