#ifndef UI_LOGIC_UI_RR_H
#define UI_LOGIC_UI_RR_H

#include "ui_types.h"

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*
 * The respiratory-rate screen: the operator counts breaths and picks a band.
 *
 * WHY IT IS TYPED IN AT ALL: the microphone that was meant to measure RR is not
 * on the board, and the model needs the feature. A band the operator can hit in
 * one press beats a field the box cannot fill.
 *
 * WHY NOT XML LIKE THE AUTHORED SCREENS: a new screen needs a generated
 * `*_gen.c` and a matching entry in the generated `ui_gen.c`, and both come out
 * of the Editor on a human pressing Ctrl+B. Hand-editing either is the one rule
 * this tree does not bend, so the alternative was to ship nothing until someone
 * opened the Editor. Same trade as ui_airway.h, and the same escape: this is its
 * four-row sibling, it instantiates the SAME generated components the XML
 * screens do -- status_bar_create(), button_bar_create() -- and transcribes
 * age.xml's styles, so the physical ButtonBar works on it. That last part is
 * what matters: the four buttons are how this box is operated in the field.
 *
 * ponytail: when RR is next authored properly in the Editor, delete this file
 * and drop `rr` from screen_root()'s special case; the rest of the wiring
 * (ui_nav, ui_action, session) stays as it is.
 */

/* Creates it on first call and caches it, so ui_nav can load it repeatedly. */
lv_obj_t *ui_rr_screen(void);

/*
 * ui_rr_band_t and ui_rr_band_value() are NOT here: they live in ui_types.h,
 * beside ui_age_band_t, and they have to. This header pulls in lvgl.h, while
 * ui_nav.c / ui_session.c / ui_action.c are deliberately LVGL-free -- they are
 * compiled by tools/run_selftests.sh without LVGL on the host. The pending band
 * and the committed band belong to those three, so putting the type behind an
 * LVGL include would drag LVGL into the logic layer and break the selftests.
 *
 * The contract with the wiring is the row order in ui_rr.c: the rows are that
 * enum's four values, ascending, and their names are opt_rr_under_12,
 * opt_rr_12_20, opt_rr_21_30, opt_rr_over_30.
 */

#endif /* UI_LOGIC_UI_RR_H */
