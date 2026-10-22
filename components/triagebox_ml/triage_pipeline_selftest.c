/*
 * Pins the REAL generated model (triage_pipeline.c), which tb_triage_selftest.c
 * deliberately does not link: that one stubs predict_triage() to test the
 * adapter and the colour mapping, so nothing else ever runs the trees.
 *
 * The expected values come from the Python LightGBM model the C was generated
 * from (predict_proba on the same raw inputs). They prove the transcription of
 * the trees and the scaler constants, not that the model is clinically right.
 * If the model is regenerated these change on purpose -- regenerate them too.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "triage_pipeline.h"

static const struct {
    float in[6]; /* age, sex, sbp, hr, rr, spo2 */
    float probs[4];
    int esi;
} k_golden[] = {
    {{52, 1, 130, 87, 18, 96}, {0.010577f, 0.049206f, 0.623488f, 0.316729f}, 3},
    {{31, 1, 120, 80, 16, 98}, {0.001025f, 0.002829f, 0.104214f, 0.891931f}, 4},
    {{70, 0, 85, 125, 28, 88}, {0.704889f, 0.293100f, 0.001416f, 0.000595f}, 1},
    {{66, 0, 100, 110, 24, 92}, {0.010580f, 0.948482f, 0.040026f, 0.000913f}, 2},
    {{25, 1, 118, 72, 14, 99}, {0.000679f, 0.001375f, 0.050505f, 0.947440f}, 4},
    {{45, 0, 60, 170, 36, 78}, {0.999610f, 0.000209f, 0.000111f, 0.000070f}, 1},
};

int main(void)
{
    for (unsigned i = 0; i < sizeof(k_golden) / sizeof(k_golden[0]); i++) {
        TriageInput in = {k_golden[i].in[0], k_golden[i].in[1], k_golden[i].in[2],
                          k_golden[i].in[3], k_golden[i].in[4], k_golden[i].in[5], 0};
        TriageOutput out = predict_triage(&in);
        float sum = 0.0f;

        assert(out.predicted_esi == k_golden[i].esi);
        for (int k = 0; k < 4; k++) {
            assert(fabsf(out.probs[k] - k_golden[i].probs[k]) < 1e-4f);
            sum += out.probs[k];
        }
        assert(fabsf(sum - 1.0f) < 1e-4f);
    }
    /* All four classes reachable: a model that can only say ESI 3 passes the
     * rows above by luck if they are ever edited carelessly. */
    int seen[5] = {0};
    for (unsigned i = 0; i < sizeof(k_golden) / sizeof(k_golden[0]); i++) {
        seen[k_golden[i].esi]++;
    }
    assert(seen[1] && seen[2] && seen[3] && seen[4]);

    printf("triage_pipeline_selftest: OK (%u golden vectors)\n",
           (unsigned)(sizeof(k_golden) / sizeof(k_golden[0])));
    return 0;
}
