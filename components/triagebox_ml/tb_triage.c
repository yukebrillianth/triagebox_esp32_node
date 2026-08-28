#include "tb_triage.h"

#include <string.h>

/*
 * Deliberately free of any triage_pipeline.h include: that header defines
 * non-static functions, so it can only be included by one translation unit
 * (tb_triage_model.c). Keeping the window maths and the ESI mapping out of that
 * unit is what lets tb_triage_selftest.c run them on the host without dragging
 * in the 49k-line model.
 */

void tb_vitals_window_reset(tb_vitals_window_t *w)
{
    if (w != NULL) {
        memset(w, 0, sizeof(*w));
    }
}

/* First valid sample seeds min and max; a zeroed struct would otherwise pin
 * every minimum at 0 and every maximum at the first reading. */
static void fold(uint16_t value, uint16_t *min, uint16_t *max, uint32_t *sum,
                 bool first)
{
    if (first || value < *min) {
        *min = value;
    }
    if (first || value > *max) {
        *max = value;
    }
    *sum += value;
}

void tb_vitals_window_add(tb_vitals_window_t *w, const vitals_t *v)
{
    bool first;

    if (w == NULL || v == NULL || !v->valid) {
        return;
    }
    first = (w->samples == 0U);

    fold(v->hr, &w->hr_min, &w->hr_max, &w->hr_sum, first);
    fold(v->spo2, &w->spo2_min, &w->spo2_max, &w->spo2_sum, first);
    fold(v->rr, &w->rr_min, &w->rr_max, &w->rr_sum, first);
    fold(v->bp_sys, &w->sbp_min, &w->sbp_max, &w->sbp_sum, first);

    if (w->samples < UINT16_MAX) {
        w->samples++;
    }
}

ui_priority_t tb_triage_esi_to_priority(int esi)
{
    switch (esi) {
    case 1: /* resuscitation */
    case 2: /* emergent */
        return UI_PRIORITY_RED;
    case 3: /* urgent */
        return UI_PRIORITY_YELLOW;
    case 4: /* less urgent */
    case 5: /* non-urgent */
        return UI_PRIORITY_GREEN;
    default:
        /* 0 is the pipeline's "cannot score" sentinel; anything else is a bug
         * upstream and must not be reported as a treatable colour. */
        return UI_PRIORITY_BLACK;
    }
}

