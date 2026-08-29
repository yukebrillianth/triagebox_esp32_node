#ifndef UI_LOGIC_UI_AIRWAY_H
#define UI_LOGIC_UI_AIRWAY_H

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*
 * The Airway screen, built in C instead of authored in the LVGL Editor.
 *
 * WHY NOT XML LIKE THE OTHER EIGHT: a new screen needs a generated `*_gen.c` and
 * a matching entry in the generated `ui_gen.c`, and both come out of the Editor
 * on a human pressing Ctrl+B. Hand-editing either is the one rule this tree does
 * not bend, so the alternative was to ship nothing until someone opened the
 * Editor. It instantiates the SAME generated components the XML screens do --
 * status_bar_create(), button_bar_create() -- and copies gender.xml's styles, so
 * it is the same screen visually and the physical ButtonBar works on it.
 *
 * Which matters more than looks: the four physical buttons are how this box is
 * operated in the field, and a dialog would have been touch-only. See
 * poll_menu_request() in ui_bindings.c for that trap.
 *
 * ponytail: when the Airway screen is next authored properly in the Editor,
 * delete this file, drop `airway` from screen_root()'s special case, and the rest
 * of the wiring (ui_nav, ui_action, session) stays as it is.
 */

/* Creates it on first call and caches it, so ui_nav can load it repeatedly. */
lv_obj_t *ui_airway_screen(void);

#endif /* UI_LOGIC_UI_AIRWAY_H */
