/*
 * The ONLY translation unit that may include tb_classify.h or
 * triage_pipeline.h: both define non-static functions in headers, so a second
 * includer is a duplicate symbol at link. Everything the host selftest needs
 * lives in tb_triage.c instead.
 */
#include "tb_triage.h"

/*
 * NULL, for this file and for tb_classify.h's own NULL checks below -- it has no
 * includes of its own and relies on whatever its single includer pulled in first.
 * ui_types.h brings stdbool/stdint, neither of which is required to define NULL:
 * it happens to arrive via newlib's headers under ESP-IDF and does NOT on the
 * host, where tb_triage_selftest.c now links this file. stddef.h is the header
 * that actually promises it.
 */
#include <stddef.h>

#include "tb_classify.h" /* pulls in triage_pipeline.h; see the warning there */

ui_priority_t tb_triage_classify(const vitals_t *v, ui_age_band_t age,
                                 ui_gender_t gender, bool airway_problem,
                                 float *confidence, int *esi)
{
    TriageInput in;
    ui_priority_t priority;
    int predicted = 0;

    if (confidence != NULL) {
        *confidence = 0.0f;
    }
    if (esi != NULL) {
        *esi = 0;
    }
    if (v == NULL || !v->valid) {
        /*
         * No complete snapshot ever arrived. tb_classify() would refuse anyway
         * on the zeroed features, but saying so here keeps the "we never
         * measured this patient" case from depending on that.
         *
         * The airway answer is deliberately NOT allowed to rescue this into RED:
         * with no vitals at all the box does not know what it is looking at, and
         * BLACK is the honest answer. An operator who has seen an obstructed
         * airway does not need the screen's permission to act on it.
         */
        return UI_PRIORITY_BLACK;
    }

    in.age = tb_triage_age_years(age);
    in.sex = tb_triage_sex(gender);
    in.heart_rate = (float)v->hr;
    in.respiratory_rate = (float)v->rr;
    in.spo2 = (float)v->spo2;
    /*
     * BP is the one vital this box does not always have: nothing on it measures
     * pressure yet, so the codec leaves bp_sys 0 -- which the refusal gate in
     * tb_classify.h (systolic_bp <= 0) read as "nobody measured this patient"
     * and answered BLACK for EVERY real patient. An absent BP is now imputed to
     * 129.7, the model's own training-set mean -- the z-score centre at
     * triage_pipeline.c:72317, x[2] = (sbp - 129.70154532) / 17.46670113 -- so
     * it scores as a perfectly average patient instead. hr/rr/spo2 still refuse
     * as before: those are measured directly, so an absence means the sensor
     * fell off, not "this patient is average". The imputation decision of
     * 2026-09-01, for the BP ML wiring, replacing the unconditional BLACK it
     * used to cause.
     */
    in.systolic_bp = (v->valid_mask & UI_VITAL_BP)
        ? (float)v->bp_sys
        : 129.7f;
    /*
     * The one input no sensor on this box can produce, so it comes from the
     * operator: the Airway screen, third of the three manual inputs. Set, it
     * forces RED regardless of the ESI -- see tb_classify.h.
     */
    in.airway_problem = airway_problem ? 1 : 0;

    priority = tb_classify(&in, &predicted);

    if (esi != NULL) {
        *esi = predicted;
    }
    if (confidence != NULL) {
        /*
         * The winning class probability. predicted_esi is 1..5 and probs[] is
         * 0..4, so the -1 is load-bearing: without it a confident ESI 5 reads as
         * the probability of ESI 4 having been chosen, which is usually near
         * zero. Bounds-checked because a refusal returns 0 and an out-of-range
         * ESI would index off the end of a 5-element array.
         *
         * ponytail: this runs the model a SECOND time, because tb_classify()
         * returns a colour and an ESI but not the probability vector it already
         * computed. Safe -- predict_triage() is pure, every value in it is a
         * local -- and it costs one extra inference per PATIENT, at the end of a
         * 60 s window, not per frame. Fix when it matters: give tb_classify() a
         * TriageOutput out-param and read probs from that.
         */
        if ((predicted >= 1) && (predicted <= 5)) {
            TriageOutput out = predict_triage(&in);

            *confidence = out.probs[predicted - 1];
        }
    }

    return priority;
}
