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

    double predicted_sbp = 0.0;
    double predicted_dbp = 0.0;

    // Isi pke bandpassed signal
    bool success = bp_predict(red_data, ir_data, ecg_data, data_length, is_male, &predicted_sbp, &predicted_dbp);

    return 0;
}
