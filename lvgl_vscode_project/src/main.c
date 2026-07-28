/**
 * @file main.c
 *
 * Minimal LVGL PC simulator.
 */

#include "lvgl.h"
#include "hal.h"

#if HAS_UI
/* Header exported by the LVGL Editor into the ui/ folder. */
#include "ui/ui.h"
#endif

#ifdef _WIN32
#include <windows.h>
static void msleep(uint32_t ms) { Sleep(ms); }
#else
#include <unistd.h>
static void msleep(uint32_t ms) { usleep(ms * 1000); }
#endif

int main(void)
{
    /*Initialize LVGL*/
    lv_init();

    /* Build everything while holding the LVGL mutex. lv_lock()/lv_unlock() are
     * no-ops when no OS is used (the SDL backend) and guard against the driver's
     * window thread on the Windows backend. */
    lv_lock();

    /*Create the display and input devices (SDL, or the Windows driver)*/
    hal_init(480, 480);

#if HAS_UI
    /* Initialize the UI exported from the LVGL Editor.
     * "A:ui" tells LVGL where the file-based assets (images, fonts) live,
     * relative to the working directory. */
    ui_init("A:ui");

    /* Load one of your screens. Replace `some_screen` with a screen name from
     * your Editor project, e.g.:
     *     lv_screen_load(home_create());
     */
     /*lv_screen_load(some_screen_create());*/
#else
    /*No UI exported yet: show a welcome message so the project runs out of the box.*/
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label,
                      "LVGL is running!\n"
                      "Export your UI from the LVGL Editor\n"
                      "into the ui/ folder to see it here.");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
#endif

    lv_unlock();

    /*Handle LVGL tasks until the window is closed*/
    while(1) {
        uint32_t idle_ms = lv_timer_handler();
        if(idle_ms == LV_NO_TIMER_READY) {
            idle_ms = LV_DEF_REFR_PERIOD;
        }
        msleep(idle_ms);
    }

    return 0;
}
