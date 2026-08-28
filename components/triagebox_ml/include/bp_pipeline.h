#ifndef BP_PIPELINE_H
#define BP_PIPELINE_H

#include <stddef.h>
#include <stdbool.h>
#include "bp_models.h"

#define BP_SAMPLING_RATE_HZ   100.0
#define BP_WINDOW_SECONDS     10.0
#define BP_WINDOW_SAMPLES     1000

#ifndef NUM_INPUT_FEATURES
#define NUM_INPUT_FEATURES    23
#endif

void bp_min_max_normalize(double *buffer, size_t length);

void bp_compute_derivatives(const double *input, double *v_out, double *a_out, size_t length);

bool bp_extract_features(
    const double *bandpass_ppg_red,
    const double *bandpass_ppg_ir,
    const double *bandpass_ecg,
    size_t num_samples,
    double is_male,
    double features_out[NUM_INPUT_FEATURES]
);

bool bp_predict(
    const double *bandpass_ppg_red,
    const double *bandpass_ppg_ir,
    const double *bandpass_ecg,
    size_t num_samples,
    double is_male,
    double *result_sbp
);

#endif // BP_PIPELINE_H
