#ifndef TB_CLASSIFY_H
#define TB_CLASSIFY_H

#include "triage_pipeline.h"
#include "ui_types.h"

ui_priority_t tb_classify(const TriageInput* input) {
    TriageOutput output = predict_triage(input);

    // Pengelompokan ke warna berdasarkan TTT
    if (output.predicted_esi >= 3) {
        return UI_PRIORITY_GREEN;
    } else if (output.predicted_esi == 2) {
        return UI_PRIORITY_YELLOW;
    } else {
        return UI_PRIORITY_RED;
    }
}

#endif // TB_CLASSIFY_H