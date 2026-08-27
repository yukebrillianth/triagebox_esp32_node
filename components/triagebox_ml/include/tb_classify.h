#ifndef TB_CLASSIFY_H
#define TB_CLASSIFY_H

#include "triage_pipeline.h"
#include "ui_types.h"

ui_priority_t tb_classify(const TriageInput* input) {
    TriageOutput output = predict_triage(input);
    return (ui_priority_t)output.predicted_esi;
}

#endif // TB_CLASSIFY_H