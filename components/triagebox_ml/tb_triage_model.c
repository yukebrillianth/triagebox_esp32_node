#include "tb_triage.h"

/*
 * The ONLY translation unit allowed to include triage_pipeline.h: that header
 * defines non-static functions (predict_triage_start_init,
 * predict_triage_start_continue) and a file-scope buffer, so a second includer
 * would either fail to link with duplicate symbols or silently get its own copy
 * of the smoothing state. Include it nowhere else.
 *
 * tb_classify.h is unused for the same reason, plus its cast of ESI straight to
 * ui_priority_t is wrong -- see tb_triage_esi_to_priority().
 */
#include "triage_pipeline.h"

/*
 * The model takes age in years but the UI only collects a band, so each band
 * reports its midpoint (the top band is open-ended; 70 is a working stand-in).
 * ponytail: precision lost here is a UI decision, not a model limit — swap the
 * Age screen for a number entry if the ML side finds it matters.
 */
static float age_band_years(ui_age_band_t band)
{
    switch (band) {
    case UI_AGE_BAND_6_17:  return 12.0f;
    case UI_AGE_BAND_18_45: return 31.0f;
    case UI_AGE_BAND_46_60: return 53.0f;
    default:                return 70.0f;
    }
}

ui_priority_t tb_triage_classify(const tb_vitals_window_t *w,
                                 ui_age_band_t age, ui_gender_t gender,
                                 float *confidence)
{
    TriageInput in;
    TriageOutput out;
    int esi;

    if (confidence != NULL) {
        *confidence = 0.0f;
    }
    if (w == NULL || w->samples == 0U) {
        return UI_PRIORITY_BLACK;
    }

    in.age = age_band_years(age);
    /* 1 = male, 0 = female per triage_pipeline.h. UI_GENDER_U means the
     * operator skipped the question; 0.5 keeps it off both poles rather than
     * silently asserting a sex. */
    in.gender = (gender == UI_GENDER_M) ? 1.0f
              : (gender == UI_GENDER_F) ? 0.0f : 0.5f;
    /* Chief complaint "breathing difficulty" is not collected anywhere in the
     * UI, and inventing it would move the prediction on made-up evidence. */
    in.cc_breathingdifficulty = 0.0f;

    in.triage_vital_hr  = (float)w->hr_sum / w->samples;
    in.triage_vital_o2  = (float)w->spo2_sum / w->samples;
    in.triage_vital_rr  = (float)w->rr_sum / w->samples;
    in.triage_vital_sbp = (float)w->sbp_sum / w->samples;

    in.pulse_min = (float)w->hr_min;
    in.pulse_max = (float)w->hr_max;
    in.spo2_min  = (float)w->spo2_min;
    in.spo2_max  = (float)w->spo2_max;
    in.resp_min  = (float)w->rr_min;
    in.resp_max  = (float)w->rr_max;
    in.sbp_min   = (float)w->sbp_min;
    in.sbp_max   = (float)w->sbp_max;

    /*
     * Mirrors the guard in predict_triage_start_init(): a zero minimum means
     * that sensor never reported, and the model must not score a partial
     * vector. Checked here rather than calling that function so the
     * probabilities behind the decision are available for confidence.
     */
    if (in.pulse_min < 1.0f || in.resp_min < 1.0f ||
        in.spo2_min < 1.0f || in.sbp_min < 1.0f) {
        return UI_PRIORITY_BLACK;
    }

    out = predict_triage(&in);
    esi = out.predicted_esi;

    if (confidence != NULL && esi >= 1 && esi <= 5) {
        *confidence = out.probs[esi - 1];
    }
    return tb_triage_esi_to_priority(esi);
}
