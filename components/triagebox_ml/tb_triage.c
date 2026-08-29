#include "tb_triage.h"

/*
 * Deliberately free of any triage_pipeline.h / tb_classify.h include: those
 * define non-static functions in headers, so they can only be included by one
 * translation unit (tb_triage_model.c). Keeping the band conversions out of that
 * unit is what lets tb_triage_selftest.c run them on the host without dragging
 * in the 72k-line model.
 */

float tb_triage_age_years(ui_age_band_t band)
{
    switch (band) {
    case UI_AGE_BAND_6_17:    return 12.0f; /* 6..17  */
    case UI_AGE_BAND_18_45:   return 31.0f; /* 18..45 */
    case UI_AGE_BAND_46_60:   return 53.0f; /* 46..60 */
    case UI_AGE_BAND_OVER_60: return 70.0f; /* 60+, no upper edge to halve */
    default:
        /* Not reachable through the UI, which only ever commits one of the four.
         * 31 rather than 0: an out-of-range band is an upstream bug, and a
         * 0-year-old would push the model far outside its training data while a
         * young adult keeps the prediction in a sane region. */
        return 31.0f;
    }
}

float tb_triage_sex(ui_gender_t gender)
{
    switch (gender) {
    case UI_GENDER_M: return 1.0f;
    case UI_GENDER_F: return 0.0f;
    default:          return 0.5f; /* UNKNOWN: contribute the mean, assert nothing */
    }
}
