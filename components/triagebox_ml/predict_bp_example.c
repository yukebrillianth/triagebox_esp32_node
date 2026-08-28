/**
 * @file predict_bp_example.c
 * @brief Complete ANSI C Demonstration Program for Cuff-Less Blood Pressure Prediction (SBP)
 *
 * Tests the end-to-end bp_pipeline on pre-bandpassed signals from the Filtered Dataset.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "bp_pipeline.h"
#include "sample_signals.h"

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    const double *red_data = SAMPLE_PPG_RED;
    const double *ir_data  = SAMPLE_PPG_IR;
    const double *ecg_data = SAMPLE_ECG_LEAD_I;
    size_t data_length     = SAMPLE_SIGNAL_LEN;
    double is_male         = SAMPLE_IS_MALE;

    double sbp_estimated;

    bool success = bp_predict(red_data, ir_data, ecg_data, data_length, is_male, &sbp_estimated);

    return 0;
}
