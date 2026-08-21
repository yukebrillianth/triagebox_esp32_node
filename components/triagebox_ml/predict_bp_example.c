#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "bp_pipeline.h"
#include "sample_signals.h"

static void print_banner(const char *title) {
    printf("\n================================================================================\n");
    printf("   %s\n", title);
    printf("================================================================================\n");
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    print_banner("CUFF-LESS BLOOD PRESSURE INFERENCE ENGINE (ANSI C / m2cgen)");
    printf("Firmware Target   : Standalone ANSI C Embedded Inference (SBP & DBP)\n");
    printf("Model Architecture: LightGBM Regressor Decision Trees (Pure C, Zero Dependencies)\n");
    printf("Feature Vector    : %d Physiological Biomarkers (Pure Waveforms, NO Age)\n", NUM_INPUT_FEATURES);

    const double *red_data = SAMPLE_PPG_RED;
    const double *ir_data  = SAMPLE_PPG_IR;
    const double *ecg_data = SAMPLE_ECG_LEAD_I;
    size_t data_length     = SAMPLE_SIGNAL_LEN;
    double is_male         = SAMPLE_IS_MALE;

    printf("\nTesting on Subject %s from Clinical Dataset (10.0s Window @ 100 Hz)...\n", SAMPLE_SUBJECT_ID);
    printf("[1/4] Running 100 Hz Biquad Filters (PPG 0.2-10Hz, ECG 0.5-35Hz)...\n");
    printf("[2/4] Detecting QRS complexes, systolic peaks, feet, and maximum velocities...\n");
    printf("[3/4] Extracting %d multi-domain physiological features...\n", NUM_INPUT_FEATURES);
    printf("[4/4] Executing LightGBM SBP and DBP Decision Tree Ensembles in C...\n");

    bp_prediction_result_t bp_result;
    bool success = bp_predict_from_raw(red_data, ir_data, ecg_data, data_length, is_male, &bp_result);

    if (!success) {
        printf("\n>>> ERROR: Blood pressure estimation failed due to insufficient signal quality. <<<\n");
        return 1;
    }

    print_banner("EXTRACTED PHYSIOLOGICAL BIOMARKERS & TIMING PARAMETERS");
    printf("  Estimated Heart Rate       : %6.1f BPM (RR interval: %.1f ms)\n", 
           bp_result.heart_rate_bpm, (60.0 / bp_result.heart_rate_bpm) * 1000.0);
    printf("  Detected Cardiac Cycles    : %6d beats\n", bp_result.num_detected_beats);
    printf("  PAT (R-Peak to Pulse Foot) : %6.2f ms\n", bp_result.pat_foot_ms);
    printf("  PAT (R-Peak to Sys Peak)   : %6.2f ms\n", bp_result.pat_peak_ms);
    printf("  PTT (Inter-channel Red-IR) : %6.2f ms\n", bp_result.ptt_inter_peak_ms);
    printf("  Pulse Width at 50%% Height  : %6.2f ms\n", bp_result.pulse_width_50_ms);
    printf("  Vascular Stiffness Ratio   : %6.4f (Tsys / Tdia)\n", bp_result.stiffness_k_val);
    printf("  Red/IR Optical Ratio (R)   : %6.4f\n", bp_result.optical_ratio_r);

    print_banner("ESTIMATED BLOOD PRESSURE INFERENCE RESULTS");
    printf("  Systolic Blood Pressure  (SBP) : %7.2f mmHg\n", bp_result.sbp);
    printf("  Diastolic Blood Pressure (DBP) : %7.2f mmHg\n", bp_result.dbp);
    printf("  Mean Arterial Pressure   (MAP) : %7.2f mmHg (Analytically calculated)\n", bp_result.map_calc);
    printf("  Pulse Pressure           (PP)  : %7.2f mmHg\n", bp_result.sbp - bp_result.dbp);
    printf("  Clinical Category (AHA/ACC)    : %s\n", bp_result.aha_category);

    printf("\n--- Clinical Ground Truth Comparison (Subject %s) ---\n", SAMPLE_SUBJECT_ID);
    printf("  Reference SBP : %7.2f mmHg (Error: %+.2f mmHg)\n", SAMPLE_TRUE_SBP, bp_result.sbp - SAMPLE_TRUE_SBP);
    printf("  Reference DBP : %7.2f mmHg (Error: %+.2f mmHg)\n", SAMPLE_TRUE_DBP, bp_result.dbp - SAMPLE_TRUE_DBP);
    printf("  Reference MAP : %7.2f mmHg (Error: %+.2f mmHg)\n", SAMPLE_TRUE_MAP, bp_result.map_calc - SAMPLE_TRUE_MAP);
    printf("  Reference HR  : %7.1f BPM  (Diff : %+.1f BPM)\n", SAMPLE_TRUE_HR, bp_result.heart_rate_bpm - SAMPLE_TRUE_HR);

    print_banner("C FIRMWARE PIPELINE EXECUTION COMPLETED SUCCESSFULLY");
    return 0;
}
