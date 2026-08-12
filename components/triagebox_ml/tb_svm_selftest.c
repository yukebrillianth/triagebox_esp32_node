/*
 * Host selftest for the SVM. Build+run: tools/run_selftests.sh
 * Not linked into firmware.
 *
 * The model is a compile-time constant, so these tests check invariants that
 * must hold for ANY exported model — they keep passing after the placeholder
 * in tb_svm_model.h is replaced with real weights.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "tb_svm.h"
#include "tb_svm_model.h"

static vitals_t sample(uint16_t hr, uint16_t spo2, uint16_t rr, uint16_t sys, uint16_t dia)
{
    vitals_t v = {
        .hr = hr, .spo2 = spo2, .rr = rr, .bp_sys = sys, .bp_dia = dia,
        .battery = 80, .valid = true,
    };
    return v;
}

/* Independent reimplementation of the scoring path: if tb_svm.c and this
 * disagree, one of them has the feature order or scaler wrong. */
static int expected_argmax(const vitals_t *v)
{
    const float x[TB_SVM_N_FEATURES] = {
        (float)v->hr, (float)v->spo2, (float)v->rr, (float)v->bp_sys, (float)v->bp_dia,
    };
    float best_score = 0.0f;
    int best = 0;

    for (int c = 0; c < TB_SVM_N_CLASSES; c++) {
        float acc = K_B[c];
        for (int i = 0; i < TB_SVM_N_FEATURES; i++) {
            float sd = K_STD[i];
            float z = (sd != 0.0f) ? ((x[i] - K_MEAN[i]) / sd) : 0.0f;
            acc += K_W[c][i] * z;
        }
        if (c == 0 || acc > best_score) {
            best_score = acc;
            best = c;
        }
    }
    return best;
}

static void test_matches_manual_argmax(void)
{
    const vitals_t cases[] = {
        sample(90, 98, 18, 120, 80),   /* nominal */
        sample(140, 88, 32, 80, 50),   /* shock-like */
        sample(45, 99, 8, 150, 95),    /* bradycardic, hypertensive */
        sample(0, 0, 0, 0, 0),         /* all-zero but flagged valid */
        sample(65535, 100, 60, 250, 160), /* saturated u16 */
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        float conf = -1.0f;
        ui_priority_t got = tb_svm_classify(&cases[i], &conf);

        assert((int)got == expected_argmax(&cases[i]));
        assert(conf >= 0.0f && conf <= 1.0f);
        assert(!isnan(conf) && !isinf(conf));
    }
}

static void test_invalid_vitals_are_black(void)
{
    /* Absent readings must not be triaged as if they were measured. */
    vitals_t v = sample(90, 98, 18, 120, 80);
    float conf = -1.0f;

    v.valid = false;
    assert(tb_svm_classify(&v, &conf) == UI_PRIORITY_BLACK);
    assert(conf == 0.0f);

    conf = -1.0f;
    assert(tb_svm_classify(NULL, &conf) == UI_PRIORITY_BLACK);
    assert(conf == 0.0f);
}

static void test_null_confidence_is_allowed(void)
{
    vitals_t v = sample(90, 98, 18, 120, 80);
    (void)tb_svm_classify(&v, NULL);
}

static void test_deterministic(void)
{
    vitals_t v = sample(112, 93, 24, 100, 65);
    float c1 = 0.0f, c2 = 0.0f;

    ui_priority_t a = tb_svm_classify(&v, &c1);
    ui_priority_t b = tb_svm_classify(&v, &c2);
    assert(a == b && c1 == c2);
}

static void test_confidence_floor(void)
{
    /* Softmax over 4 classes cannot be more confident than 1.0 nor less than
     * 1/4 (all scores equal) — a value below that means the math is wrong. */
    vitals_t v = sample(90, 98, 18, 120, 80);
    float conf = 0.0f;

    (void)tb_svm_classify(&v, &conf);
    assert(conf >= 0.25f - 1e-4f);
}

static void test_scaler_export_sane(void)
{
    /* A zero std means the notebook exported a constant feature it should have
     * dropped. tb_svm.c guards it, but flag it here so it is noticed. */
    for (int i = 0; i < TB_SVM_N_FEATURES; i++) {
        if (K_STD[i] == 0.0f) {
            printf("tb_svm_selftest: WARNING K_STD[%d]==0 (constant feature?)\n", i);
        }
    }
}

int main(void)
{
    test_matches_manual_argmax();
    test_invalid_vitals_are_black();
    test_null_confidence_is_allowed();
    test_deterministic();
    test_confidence_floor();
    test_scaler_export_sane();
    printf("tb_svm_selftest: OK\n");
    return 0;
}
