#ifndef BP_PIPELINE_H
#define BP_PIPELINE_H

#include <stddef.h>
#include <stdbool.h>
#include "bp_models.h"
#include "ppg_bandpass_filter.h"
#include "ecg_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BP_SAMPLING_RATE_HZ 100.0
#define BP_WINDOW_SAMPLES   1000 

typedef struct {
    double sbp;               /**< Systolic Blood Pressure (mmHg) */
    double dbp;               /**< Diastolic Blood Pressure (mmHg) */
    double map_calc;          /**< Analytically Calculated Mean Arterial Pressure (DBP + 1/3*(SBP-DBP)) */
    double heart_rate_bpm;    /**< Estimated Heart Rate (BPM) */
    double pat_foot_ms;       /**< Mean Pulse Arrival Time to Foot (ms) */
    double pat_peak_ms;       /**< Mean Pulse Arrival Time to Peak (ms) */
    double ptt_inter_peak_ms; /**< Inter-channel Red-IR Transit Time (ms) */
    double pulse_width_50_ms; /**< PPG 50% Pulse Width (ms) */
    double stiffness_k_val;   /**< Vascular Stiffness Index (Tsys / Tdia) */
    double optical_ratio_r;   /**< Red / IR Optical Ratio */
    int num_detected_beats;   /**< Number of valid detected cardiac cycles */
    const char *aha_category; /**< AHA / ACC Blood Pressure Classification String */
} bp_prediction_result_t;

void bp_min_max_normalize(double *buffer, size_t length);
void bp_compute_derivatives(const double *input, double *v_out, double *a_out, size_t length);

bool bp_extract_features(
    const double *raw_ppg_red,
    const double *raw_ppg_ir,
    const double *raw_ecg,
    size_t num_samples,
    double is_male,
    double features_out[NUM_INPUT_FEATURES]
);

bool bp_predict_from_raw(
    const double *raw_ppg_red,
    const double *raw_ppg_ir,
    const double *raw_ecg,
    size_t num_samples,
    double is_male,
    bp_prediction_result_t *result
);

#ifdef __cplusplus
}
#endif

#endif // BP_PIPELINE_H
