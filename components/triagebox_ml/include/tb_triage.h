#ifndef TB_TRIAGE_H
#define TB_TRIAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_types.h"

/*
 * Adapter between the UI and the ML pipeline in this component.
 *
 * It exists so exactly one reviewable place owns the conversions that are easy
 * to get silently wrong, and so nothing in ui/logic/ has to know that ESI or
 * TriageInput exist:
 *
 *   1. ESI (1..5, clinical severity, 1 = worst) -> ui_priority_t (START colour).
 *      Different scales, different directions, and ui_priority_t's numeric order
 *      (RED=0, YELLOW=1, GREEN=2, BLACK=3) is NOT severity order, so a cast
 *      silently maps the most critical patient to the least urgent colour. The
 *      mapping itself belongs to the ML side and lives in their tb_classify.h;
 *      this adapter is what calls it, and tb_triage_selftest.c is what pins it.
 *   2. ui_age_band_t / ui_gender_t -> the model's float age and sex. The UI only
 *      ever collects a band, so a single number has to be chosen for it.
 *
 * The model takes an INSTANTANEOUS reading -- seven scalars, no window
 * aggregates. That is a change from the previous pipeline, which wanted
 * mean/min/max over the measure window; tb_vitals_window_t existed only for
 * that and is gone with it.
 */

/*
 * Classify one patient.
 *
 * @param v           the latest snapshot. v->valid clear, or any feature at 0,
 *                    means refuse to score: BLACK with confidence 0. Guessing a
 *                    priority from vitals that were never measured is worse than
 *                    admitting the box does not know.
 * @param age,gender  as committed on the Age/Gender screens.
 * @param airway_problem
 *                    as committed on the Airway screen. The ONE input that
 *                    overrides the model: set, the result is RED whatever the ESI
 *                    came out as. It is an operator judgement, not a reading --
 *                    no sensor on this box can see an obstructed airway.
 * @param confidence  may be NULL; else the winning class probability, 0..1 --
 *                    the range the backend expects.
 * @param esi         may be NULL; else the raw ESI 1..5, or 0 when it refused.
 *                    Exposed because it is the model's actual output and three
 *                    colours cannot be un-collapsed back into it: ESI 3, 4 and 5
 *                    are all GREEN, so a log line carrying only the colour
 *                    cannot tell a walking-wounded patient from a borderline
 *                    one. It also keeps the airway override auditable -- RED with
 *                    esi=5 is the override, RED with esi=1 is the model.
 *                    Diagnostics only -- nothing on screen reads it.
 */
ui_priority_t tb_triage_classify(const vitals_t *v, ui_age_band_t age,
                                 ui_gender_t gender, bool airway_problem,
                                 float *confidence, int *esi);

/*
 * The two band conversions, exposed for the selftest.
 *
 * Midpoints, not edges: the model was trained on real ages, so the band has to
 * become one number, and the middle is the choice that bounds the error at half
 * the band's width instead of all of it. UI_AGE_BAND_OVER_60 has no upper edge,
 * so 70 is a judgement -- old enough to carry the risk the band implies, young
 * enough not to extrapolate past the training data.
 */
float tb_triage_age_years(ui_age_band_t band);

/*
 * 1.0 male, 0.0 female, and 0.5 for UNKNOWN -- deliberately between the two
 * rather than defaulting to either. A sex the operator never entered should not
 * be asserted as one of them, and 0.5 is what a linear feature does with "no
 * information": it contributes the mean.
 */
float tb_triage_sex(ui_gender_t gender);

#endif /* TB_TRIAGE_H */
