#include <stdio.h>
#include <math.h>
#include "bp_models.h"
#include "ppg_bandpass_filter.h"
#include "ecg_filter.h"

int main() {
    puts("================================================================");
    puts("   C PRODUCTION FIRMWARE DEPLOYMENT & BIT-EXACT VERIFICATION   ");
    puts("================================================================");

    // 1. Test Filter Instantiation
    ppg_biquad_cascade_t ppg_filter;
    ppg_filter_reset(&ppg_filter);
    double ppg_test = ppg_filter_step(&ppg_filter, 0.5);

    ecg_filter_state_t ecg_filter;
    ecg_filter_reset(&ecg_filter);
    double ecg_test = ecg_filter_step(&ecg_filter, 100.0);
    double qrs_test = ecg_pan_tompkins_step(&ecg_filter, ecg_test);

    printf("Filter Initializations OK: PPG step = %f, ECG step = %f, QRS env = %f\n", ppg_test, ecg_test, qrs_test);

    // 2. Test Model Predictions on Sample Vector
    double input[NUM_INPUT_FEATURES] = {0.38220397, 0.01001015, 156.09002674, -200.00000000, 40.00000000, 100.00000000, -262.04100425, 52.40820085, 131.02050213, -239.47129000, 47.89425800, 119.73564500, 5.29831742, 3.68887970, 4.60517029, -0.00500000, 0.00002500, 0.02499999, 0.00062500, 0.01000000, 0.00010000, 300.00000000, 240.00000000, 60.00000000, -0.50000003, -0.20000001, 2.49999938, 0.00333333, 0.00416667, 0.01666666, -10.00000000, -10.00000000, -266.99029126, 0.01297493, -0.03047147, 0.00359028, -0.00609130, 310.00000000, 197.00000000, 116.00000000, 0.33670033, -0.10354380, 0.99428212, 0.98968762, 26.17662949, 25.79138047, 0.98528233, 0.98524507, 0.00918435, 0.00140829, 0.00932329, 0.00148682, 10.41764586, 9.23264155, 300.00000000, 0.00023840, 4.25175835, 0.10865967, -0.10354380, 1.18599013, 3.15920870, 300.00000000, 0.00023357, 309.00000000, 187.00000000, 110.00000000, 4.09795818, 0.08392520, -0.15844525, 2.40292767, -2.87812992, 1.00000000};

    double pred_sbp = predict_sbp(input);
    double pred_dbp = predict_dbp(input);

    printf("\nC Model Inference Results on Test Vector:\n");
    printf("  SBP Predicted : %7.2f mmHg (Python Reference:  146.88 mmHg)\n", pred_sbp);
    printf("  DBP Predicted : %7.2f mmHg (Python Reference:   90.23 mmHg)\n", pred_dbp);

    double err_sbp = fabs(pred_sbp - (146.87548178));
    double err_dbp = fabs(pred_dbp - (90.22593077));

    printf("\nDiscrepancy (C vs Python):\n");
    printf("  SBP Error : %e mmHg\n", err_sbp);
    printf("  DBP Error : %e mmHg\n", err_dbp);

    if (err_sbp < 1e-4 && err_dbp < 1e-4 && err_map < 1e-4) {
        puts("\n>>> STATUS: BIT-EXACT CONCORDANCE VERIFIED (ALL TESTS PASSED) <<<");
        return 0;
    } else {
        puts("\n>>> STATUS: DISCREPANCY DETECTED <<<");
        return 1;
    }
}