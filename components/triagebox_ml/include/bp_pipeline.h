#ifndef BP_PIPELINE_H
#define BP_PIPELINE_H

#include <stddef.h>
#include <stdbool.h>
#include "bp_models.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BP_SAMPLING_RATE_HZ 100.0

void bp_zscore_normalize(double *buffer, size_t length);
void bp_compute_derivatives(const double *input, double *v_out, double *a_out, size_t length);

bool bp_extract_features(
    const double *bandpass_ppg_red,
    const double *bandpass_ppg_ir,
    const double *bandpass_ecg,
    size_t num_samples,
    double is_male,
    double *features_out
);

bool bp_predict(
    const double *bandpass_ppg_red,
    const double *bandpass_ppg_ir,
    const double *bandpass_ecg,
    size_t num_samples,
    double is_male,
    double *result_sbp,
    double *result_dbp
);

#ifdef __cplusplus
}
#endif

#endif // BP_PIPELINE_H
