#ifndef TB_CLASSIFY_H
#define TB_CLASSIFY_H

#include "triage_pipeline.h"
#include "ui_types.h"

ui_priority_t tb_classify(const TriageInput* input) {
    int best = predict_triage_start_init(input);
    return (ui_priority_t)best;
}

#endif // TB_CLASSIFY_H