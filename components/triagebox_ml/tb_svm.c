#include "tb_svm.h"

#include <math.h>
#include <stddef.h>

#include "tb_svm_model.h"

ui_priority_t tb_svm_classify(const vitals_t *v, float *confidence)
{
    float z[TB_SVM_N_FEATURES];
    float score[TB_SVM_N_CLASSES];
    float max_score;
    float sum_exp = 0.0f;
    int best = 0;

    if (v == NULL || !v->valid) {
        if (confidence != NULL) {
            *confidence = 0.0f;
        }
        return UI_PRIORITY_BLACK;
    }

    /* StandardScaler. K_STD comes from the training export; a zero there would
     * mean a constant feature, which the notebook should have dropped — guard
     * anyway so a bad export cannot produce inf/NaN scores. */
    const float x[TB_SVM_N_FEATURES] = {
        (float)v->hr, (float)v->spo2, (float)v->rr, (float)v->bp_sys, (float)v->bp_dia,
    };
    for (int i = 0; i < TB_SVM_N_FEATURES; i++) {
        float sd = K_STD[i];
        z[i] = (sd != 0.0f) ? ((x[i] - K_MEAN[i]) / sd) : 0.0f;
    }

    for (int c = 0; c < TB_SVM_N_CLASSES; c++) {
        float acc = K_B[c];
        for (int i = 0; i < TB_SVM_N_FEATURES; i++) {
            acc += K_W[c][i] * z[i];
        }
        score[c] = acc;
        if (acc > score[best]) {
            best = c;
        }
    }

    if (confidence != NULL) {
        /* Softmax, shifted by the max so exp() cannot overflow. A one-vs-rest
         * SVM has no calibrated probability; this is a monotone score-to-[0,1]
         * map so the backend's 0..1 field is populated sensibly. */
        max_score = score[best];
        for (int c = 0; c < TB_SVM_N_CLASSES; c++) {
            sum_exp += expf(score[c] - max_score);
        }
        *confidence = (sum_exp > 0.0f) ? (1.0f / sum_exp) : 0.0f;
    }

    return (ui_priority_t)best;
}
