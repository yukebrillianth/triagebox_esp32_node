/*
 * The ONLY translation unit that may include tb_classify.h or
 * triage_pipeline.h: both define non-static functions in headers, so a second
 * includer is a duplicate symbol at link. Everything the host selftest needs
 * lives in tb_triage.c instead.
 */
#include "tb_triage.h"

#include "tb_classify.h" /* pulls in triage_pipeline.h; see the warning there */

ui_priority_t tb_triage_classify(const vitals_t *v, ui_age_band_t age,
                                 ui_gender_t gender, float *confidence,
                                 int *esi)
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
        /* No complete snapshot ever arrived. tb_classify() would refuse anyway
         * on the zeroed features, but saying so here keeps the "we never
         * measured this patient" case from depending on that. */
        return UI_PRIORITY_BLACK;
    }

    in.age = tb_triage_age_years(age);
    in.sex = tb_triage_sex(gender);
    in.heart_rate = (float)v->hr;
    in.respiratory_rate = (float)v->rr;
    in.spo2 = (float)v->spo2;
    in.systolic_bp = (float)v->bp_sys;
    /*
     * Hardcoded 0, and this is the one input worth flagging: airway_problem
     * forces RED on its own, so with nothing ever setting it that override can
     * never fire. Nothing in the UI collects it -- there is no airway question on
     * the Age/Gender screens and no bit for it on the I2C link. Wiring it up
     * means a new registration step or a new STM32 flag, not a change here.
     */
    in.airway_problem = 0;

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
