#include "triage_pipeline.h"

// Contoh penggunaan model

void demo_triage() {
    TriageInput triage_input_init;
    triage_input_init.age = 30;
    triage_input_init.cc_breathingdifficulty = 1;
    triage_input_init.gender = 1;
    triage_input_init.triage_vital_hr = 80.0;
    triage_input_init.triage_vital_sbp = 120.0;
    triage_input_init.triage_vital_rr = 16.0;
    triage_input_init.triage_vital_o2 = 98.0;
    triage_input_init.pulse_min = 70.0;
    triage_input_init.resp_min = 14.0;
    triage_input_init.spo2_min = 95.0;
    triage_input_init.sbp_min = 110.0;
    triage_input_init.pulse_max = 90.0;
    triage_input_init.resp_max = 18.0;
    triage_input_init.spo2_max = 99.0;
    triage_input_init.sbp_max = 130.0;

    TriageOutput triage_output_init = predict_triage(&triage_input_init);

    int reset_state = 0;
    while (1) {
        TriageInput triage_input_continue;
        triage_input_continue.age = 30;
        triage_input_continue.cc_breathingdifficulty = 1;
        triage_input_continue.gender = 1;
        triage_input_continue.triage_vital_hr = 80.0;
        triage_input_continue.triage_vital_sbp = 120.0;
        triage_input_continue.triage_vital_rr = 16.0;
        triage_input_continue.triage_vital_o2 = 98.0;
        triage_input_continue.pulse_min = 70.0;
        triage_input_continue.resp_min = 14.0;
        triage_input_continue.spo2_min = 95.0;
        triage_input_continue.sbp_min = 110.0;
        triage_input_continue.pulse_max = 90.0;
        triage_input_continue.resp_max = 18.0;
        triage_input_continue.spo2_max = 99.0;
        triage_input_continue.sbp_max = 130.0;

        int triage_result = predict_triage_start_continue(&triage_input_continue, reset_state);

        reset_state = 0;
    }
}