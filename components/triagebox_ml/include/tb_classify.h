#ifndef TB_CLASSIFY_H
#define TB_CLASSIFY_H

#include "triage_pipeline.h"
#include "ui_types.h"

/*
 * WARNING: this is a non-static function DEFINED in a header, so exactly ONE
 * translation unit may include it -- a second includer is a duplicate-symbol
 * error at link. In this tree that unit is tb_triage_model.c. Moving the body to
 * a tb_classify.c would remove the constraint; until then, do not include this
 * anywhere else.
 *
 * This is the ESI -> START colour mapping and it belongs to the ML side. Grouped
 * to the three colours Indonesian START uses, so ESI 3, 4 and 5 collapse into
 * GREEN together -- which is why tb_triage_classify() also reports the raw ESI.
 * Pinned by tb_triage_selftest.c against a stubbed predict_triage(), so the
 * mapping is checked without linking the 72k-line model.
 */
ui_priority_t tb_classify(const TriageInput* input, int* predicted_esi) {
    /*
     * Refuse to score rather than guess. Note this writes 0, not 1: the ESI
     * scale starts at 1 (resuscitation), so reporting 1 here would claim the
     * most critical class for a patient nobody measured. 0 is outside the scale
     * and reads as "no score".
     */
    if (input->heart_rate <= 0 || input->respiratory_rate <= 0 || input->spo2 <= 0 || input->systolic_bp <= 0) {
        if (predicted_esi != NULL) {
            *predicted_esi = 0;
        }
        return UI_PRIORITY_BLACK;
    }

    TriageOutput output = predict_triage(input);

    /* Through the pointer, not over it. `predicted_esi = output.predicted_esi;`
     * assigns an int to the pointer itself: the caller's variable is never
     * written, so every caller reads whatever it happened to be initialised
     * with, and -Werror rejects the conversion anyway. */
    if (predicted_esi != NULL) {
        *predicted_esi = output.predicted_esi;
    }

    // Pengelompokan ke warna berdasarkan TTT
    if (output.predicted_esi == 1 || input->airway_problem) {
        return UI_PRIORITY_RED;
    } else if (output.predicted_esi == 2) {
        return UI_PRIORITY_YELLOW;
    } else {
        return UI_PRIORITY_GREEN;
    }
}

#endif // TB_CLASSIFY_H