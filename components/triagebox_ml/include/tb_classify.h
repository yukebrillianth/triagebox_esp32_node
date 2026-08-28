#ifndef TB_CLASSIFY_H
#define TB_CLASSIFY_H

#include "triage_pipeline.h"
#include "ui_types.h"

ui_priority_t tb_classify(const TriageInput* input, int* predicted_esi) {
    if (input->heart_rate <= 0 || input->respiratory_rate <= 0 || input->spo2 <= 0 || input->systolic_bp <= 0) {
        predicted_esi = 1;
        return UI_PRIORITY_BLACK;
    }

    TriageOutput output = predict_triage(input);
    predicted_esi = output.predicted_esi;

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