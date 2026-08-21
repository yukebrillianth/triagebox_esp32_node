#ifndef TB_TRIAGE_H
#define TB_TRIAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_types.h"

/*
 * Adapter between the UI and the ML pipeline in this component.
 *
 * It exists so exactly one reviewable place owns the two conversions that are
 * easy to get silently wrong:
 *
 *   1. ESI (1..5, clinical severity) -> ui_priority_t (START colour). These are
 *      different scales with different directions, and ui_priority_t's numeric
 *      order (RED=0) is not severity order.
 *   2. instantaneous vitals_t -> the mean/min/max feature vector the model
 *      wants. Eight of its fifteen features are window aggregates, so a single
 *      snapshot cannot fill them.
 *
 * Nothing in ui/logic/ knows about ESI or TriageInput, and triage_pipeline.c
 * stays untouched so the ML side can keep iterating on it.
 */

/*
 * Running aggregate over one measure window. Zero it (or call
 * tb_vitals_window_reset) before the window starts, then add every VITAL
 * snapshot as it arrives.
 */
typedef struct {
    uint16_t hr_min, hr_max;
    uint16_t spo2_min, spo2_max;
    uint16_t rr_min, rr_max;
    uint16_t sbp_min, sbp_max;
    /* Sums are 32-bit: 60 s of 20 Hz samples of a 3-digit reading overflows
     * 16 bits long before the window ends. */
    uint32_t hr_sum, spo2_sum, rr_sum, sbp_sum;
    uint16_t samples;
} tb_vitals_window_t;

void tb_vitals_window_reset(tb_vitals_window_t *w);

/* Fold one snapshot in. Ignores NULL and snapshots with v->valid clear, so a
 * stale frame cannot drag a minimum down to a value no sensor produced. */
void tb_vitals_window_add(tb_vitals_window_t *w, const vitals_t *v);

/*
 * Classify one patient.
 *
 * Returns the START colour to display and send. *confidence (may be NULL) gets
 * the winning class probability, 0..1 — the range the backend expects.
 *
 * An empty window (no valid snapshot ever arrived) returns UI_PRIORITY_BLACK
 * with confidence 0: refusing to score is safer than scoring on absent vitals.
 */
ui_priority_t tb_triage_classify(const tb_vitals_window_t *w,
                                 ui_age_band_t age, ui_gender_t gender,
                                 float *confidence);

/*
 * ESI 1..5 -> START colour, exposed for the selftest.
 *
 * Mapping is the one the ML side documented in triage_pipeline.h:
 *   ESI 1,2 -> RED    (resuscitation / emergent)
 *   ESI 3   -> YELLOW (urgent)
 *   ESI 4,5 -> GREEN  (less urgent / non-urgent)
 *   0 or out of range -> BLACK (their sentinel for "cannot score")
 */
ui_priority_t tb_triage_esi_to_priority(int esi);

#endif /* TB_TRIAGE_H */
