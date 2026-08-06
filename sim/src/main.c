/**
 * @file main.c
 *
 * Minimal LVGL PC simulator.
 * Wires ui_runtime + mock keypad keys; triage screens when exported.
 */

#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include "hal.h"

#include "ui_action.h"
#include "ui_input.h"
#include "ui_mock.h"
#include "ui_nav.h"
#include "ui_runtime.h"

#if HAS_UI
/* Header exported by the LVGL Editor into the ui/ folder. */
#include "ui/ui.h"
#endif

#ifndef HAS_TRIAGE_SCREENS
#define HAS_TRIAGE_SCREENS 0
#endif

#if HAS_TRIAGE_SCREENS
/* Globals from ui_gen.h via ui.h; *_create only if ui_init skipped permanent block. */
#include "screens/home_gen.h"
#include "screens/scanning_gen.h"
#include "screens/berhasil_gen.h"
#include "screens/age_gen.h"
#include "screens/gender_gen.h"
#include "screens/mengukur_gen.h"
#include "screens/result_gen.h"
#include "screens/monitor_gen.h"
#endif

#ifdef _WIN32
#include <windows.h>
static void msleep(uint32_t ms) { Sleep(ms); }
#else
#include <unistd.h>
#include <SDL.h>
static void msleep(uint32_t ms) { usleep(ms * 1000); }
#endif

static void runtime_timer_cb(lv_timer_t *t)
{
    (void)t;
    ui_runtime_tick(lv_tick_get());
}

#if HAS_TRIAGE_SCREENS
/*
 * Permanent screens already created by ui_init_gen (LV_USE_XML==0).
 * Do NOT call *_create() again — that re-parents extra children onto the same root.
 *
 * Key walk (SDL keys 1-4 = bar cells 0-3; p/c cycle mock priority):
 *   Home:     2=Scan
 *   Scanning: (auto RFID after UI_MOCK_SCAN_MS) | 1=Abort
 *   Berhasil: 1=Start  2=Restart
 *   Age:      4=Select (3=Back)
 *   Gender:   4=Select → Mengukur (3=Back)
 *   Mengukur: (auto after UI_MEASURE_MS) | 1=Abort
 *   Result:   1=Monitor  2=Reset
 *   Monitor:  1=Back  2=Stop
 */
static void show_home(void)
{
    LV_LOG_USER("nav → HOME");
    lv_screen_load(home);
}
static void show_scanning(void)
{
    LV_LOG_USER("nav → SCANNING");
    lv_screen_load(scanning);
}
static void show_berhasil(void)
{
    LV_LOG_USER("nav → BERHASIL");
    lv_screen_load(berhasil);
}
static void show_age(void)
{
    LV_LOG_USER("nav → AGE");
    lv_screen_load(age);
}
static void show_gender(void)
{
    LV_LOG_USER("nav → GENDER");
    lv_screen_load(gender);
}
static void show_mengukur(void)
{
    LV_LOG_USER("nav → MENGUKUR");
    lv_screen_load(mengukur);
}
static void show_result(void)
{
    LV_LOG_USER("nav → RESULT");
    lv_screen_load(result);
}
static void show_monitor(void)
{
    LV_LOG_USER("nav → MONITOR");
    lv_screen_load(monitor);
}

static void register_triage_screens(void)
{
    /* Prefer screens from ui_init_gen permanent block; create once if missing. */
    if(home == NULL) {
        LV_LOG_USER("permanent screens missing after ui_init — creating once");
        home_create();
        scanning_create();
        berhasil_create();
        age_create();
        gender_create();
        mengukur_create();
        result_create();
        monitor_create();
    }

    ui_nav_register(UI_SCREEN_HOME, show_home);
    ui_nav_register(UI_SCREEN_SCANNING, show_scanning);
    ui_nav_register(UI_SCREEN_BERHASIL, show_berhasil);
    ui_nav_register(UI_SCREEN_AGE, show_age);
    ui_nav_register(UI_SCREEN_GENDER, show_gender);
    ui_nav_register(UI_SCREEN_MENGUKUR, show_mengukur);
    ui_nav_register(UI_SCREEN_RESULT, show_result);
    ui_nav_register(UI_SCREEN_MONITOR, show_monitor);

    if(home == NULL) {
        LV_LOG_ERROR("home still NULL after create — cannot load");
        return;
    }
    lv_screen_load(home);
    LV_LOG_USER("permanent screens registered (home=%p scanning=%p result=%p)",
                (void *)home, (void *)scanning, (void *)result);
}
#endif

/*
 * SDL keys 1/2/3/4 → mock buttons 0..3 (press/release edges).
 * Also dispatch ui_action for the fixed ButtonBar (not focus-routed).
 * Keys p/c → cycle mock priority for Result QA.
 * Uses GetKeyboardState so we do not steal SDL events from LVGL's driver.
 */
#ifndef _WIN32
static void sim_poll_keys(void)
{
    const Uint8 *ks = SDL_GetKeyboardState(NULL);
    static uint8_t prev[4];
    static uint8_t prev_p;
    static uint8_t prev_c;
    static const SDL_Scancode codes[4] = {
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4
    };
    uint8_t i;

    for(i = 0; i < 4; i++) {
        uint8_t down = ks[codes[i]] ? 1U : 0U;
        if(down && !prev[i]) {
            ui_mock_push_button(i, true);
            ui_action(ui_nav_current(), i);
        } else if(!down && prev[i]) {
            ui_mock_push_button(i, false);
        }
        prev[i] = down;
    }

    {
        uint8_t p = ks[SDL_SCANCODE_P] ? 1U : 0U;
        uint8_t c = ks[SDL_SCANCODE_C] ? 1U : 0U;
        if((p && !prev_p) || (c && !prev_c)) {
            ui_runtime_debug_cycle_priority();
        }
        prev_p = p;
        prev_c = c;
    }
}
#else
static void sim_poll_keys(void) { /* Win32: use on-screen touch / later hook */ }
#endif

int main(void)
{
    lv_display_t *disp;

    /*Initialize LVGL*/
    lv_init();

    /* Build everything while holding the LVGL mutex. lv_lock()/lv_unlock() are
     * no-ops when no OS is used (the SDL backend) and guard against the driver's
     * window thread on the Windows backend. */
    lv_lock();

    /*Create the display and input devices (SDL, or the Windows driver)*/
    disp = hal_init(480, 480);

    ui_runtime_init();
    ui_input_keypad_init(disp);
    lv_timer_create(runtime_timer_cb, 50, NULL);

#if HAS_TRIAGE_SCREENS
    /* Full triage export present: init Editor UI assets then permanent screens. */
#if HAS_UI
    /* Trailing slash required: asset_path + "fonts/X" → "A:ui/fonts/X". */
    ui_init("A:ui/");
#endif
    register_triage_screens();
    LV_LOG_USER("HAS_TRIAGE_SCREENS=1 — home loaded, keys 1-4 drive bar");
#elif HAS_UI
    /* Template export only (no home_gen yet). */
    ui_init("A:ui/");
    /* Optional: lv_screen_load(screen_components_create()); */
    LV_LOG_USER("triage gens missing — template ui_init only; keys 1-4 still drive nav logic");
#else
    /*No UI exported yet: show a welcome message so the project runs out of the box.*/
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label,
                      "LVGL is running!\n"
                      "Export your UI from the LVGL Editor\n"
                      "into the ui/ folder to see it here.\n"
                      "Keys 1-4 = bar; p/c = cycle priority");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    LV_LOG_USER("triage gens missing — welcome screen; logic still ticked");
#endif

    lv_unlock();

#if HAS_TRIAGE_SCREENS
    /* Optional headless walk for CI evidence (same handlers as keys 1-4). */
    {
        const char *env = getenv("SIM_AUTO_WALK");
        if(env && env[0] == '1') {
            uint32_t t0 = lv_tick_get();
            int step = 0;
            LV_LOG_USER("SIM_AUTO_WALK: start full triage key path");
            while(1) {
                uint32_t now = lv_tick_get();
                uint32_t elapsed = now - t0;
                /* Pace actions so mock scan/measure timers can fire between them. */
                if(step == 0 && elapsed > 200) {
                    ui_action(UI_SCREEN_HOME, 1); /* Scan */
                    step = 1;
                } else if(step == 1 && ui_nav_current() == UI_SCREEN_BERHASIL) {
                    ui_action(UI_SCREEN_BERHASIL, 0); /* Start → Age */
                    step = 2;
                } else if(step == 2 && ui_nav_current() == UI_SCREEN_AGE) {
                    ui_action(UI_SCREEN_AGE, 3); /* Select → Gender */
                    step = 3;
                } else if(step == 3 && ui_nav_current() == UI_SCREEN_GENDER) {
                    ui_action(UI_SCREEN_GENDER, 3); /* Select → Mengukur */
                    step = 4;
                } else if(step == 4 && ui_nav_current() == UI_SCREEN_RESULT) {
                    ui_action(UI_SCREEN_RESULT, 0); /* Monitor */
                    step = 5;
                } else if(step == 5 && ui_nav_current() == UI_SCREEN_MONITOR) {
                    LV_LOG_USER("SIM_AUTO_WALK: ALL_SCREENS_OK (reached MONITOR)");
                    step = 6;
                } else if(step == 6 && elapsed > 3000) {
                    LV_LOG_USER("SIM_AUTO_WALK: done, exit");
                    return 0;
                }
                /* Fail-safe timeout */
                if(elapsed > 15000) {
                    LV_LOG_USER("SIM_AUTO_WALK: TIMEOUT at step=%d screen=%d",
                                step, (int)ui_nav_current());
                    return 1;
                }
                uint32_t idle_ms = lv_timer_handler();
                if(idle_ms == LV_NO_TIMER_READY) {
                    idle_ms = LV_DEF_REFR_PERIOD;
                }
                msleep(idle_ms > 20 ? 20 : idle_ms);
            }
        }
    }
#endif

    /*Handle LVGL tasks until the window is closed*/
    while(1) {
        sim_poll_keys();
        uint32_t idle_ms = lv_timer_handler();
        if(idle_ms == LV_NO_TIMER_READY) {
            idle_ms = LV_DEF_REFR_PERIOD;
        }
        msleep(idle_ms);
    }

    return 0;
}
