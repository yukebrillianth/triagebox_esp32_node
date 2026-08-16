#ifndef TRIAGE_PIPELINE_H
#define TRIAGE_PIPELINE_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float age;
    float cc_breathingdifficulty;
    float gender; // 1 = male, 0 = female
    float triage_vital_hr; // HR mean
    float triage_vital_sbp; // SBP mean
    float triage_vital_rr; // RR mean
    float triage_vital_o2; // SpO2 mean
    float pulse_min; // HR min
    float resp_min; // RR min
    float spo2_min; // SpO2 min
    float sbp_min; // SBP min
    float pulse_max; // HR max
    float resp_max; // RR max
    float spo2_max; // SpO2 max
    float sbp_max; // SBP max
} TriageInput;
typedef struct {
    float probs[5];
    int predicted_esi;
} TriageOutput;
TriageOutput predict_triage(const TriageInput* input);

/*
    brief: Setelah 1 menit pertama setelah tekan tombol 'START', yang dipake buat prediksi triase yang ini, buat sementara ESI 4 5 kumasukkan hijau, ESI 3 kuning, ESI 2 1 merah
    ret: 0 = hitam, 1 = merah, 2 = kuning, 3 = hijau
*/
int predict_triage_start_init(const TriageInput* input) {
    if (input->pulse_min < 1 || input->resp_min < 1 || input->spo2_min < 1 || input->sbp_min < 1) {
        return 0; 
    }

    TriageOutput output = predict_triage(input);

    if (output.predicted_esi == 1 || output.predicted_esi == 2) {
        return 1; 
    } else if (output.predicted_esi == 3) {
        return 2; 
    } else if (output.predicted_esi == 4 || output.predicted_esi == 5) {
        return 3; 
    }
}

static float triage_probs_buffer[5] = {0.0, 0.0, 0.0, 0.0, 0.0};

/*
    brief: Setelah eksekusi predict_triage_start_init(), ini yang dipake buat update kondisi pasien selama interval tertentu (15 detik?)
    ret: 0 = hitam, 1 = merah, 2 = kuning, 3 = hijau
*/
int predict_triage_start_continue(const TriageInput* input, int reset_state) {
    if (input->pulse_min < 1 || input->resp_min < 1 || input->spo2_min < 1 || input->sbp_min < 1) {
        triage_probs_buffer[0] = 0.0;
        triage_probs_buffer[1] = 0.0;
        triage_probs_buffer[2] = 0.0;
        triage_probs_buffer[3] = 0.0;
        triage_probs_buffer[4] = 0.0;
        return 0; 
    }

    TriageOutput output = predict_triage(input);

    if (reset_state) {
        for (int i = 0; i < 5; i++) {
            triage_probs_buffer[i] = output.probs[i];
        }
    } else {
        const float alpha = 0.7;
        float probs_sum = 0.0;

        for (int i = 0; i < 5; i++) {
            triage_probs_buffer[i] = (1 - alpha) * triage_probs_buffer[i] + alpha * output.probs[i];
            probs_sum += triage_probs_buffer[i];
        }

        for (int i = 0; i < 5; i++) {
            triage_probs_buffer[i] /= probs_sum;
        }
    }

    int current_esi = 1;
    float max_prob = triage_probs_buffer[0];
    for (int i = 1; i < 5; i++) {
        if (triage_probs_buffer[i] > max_prob) {
            max_prob = triage_probs_buffer[i];
            current_esi = i + 1;
        }
    }

    if (current_esi == 1 || current_esi == 2) {
        return 1; 
    } else if (current_esi == 3) {
        return 2; 
    } else if (current_esi == 4 || current_esi == 5) {
        return 3; 
    }
}

#ifdef __cplusplus
}
#endif
#endif // TRIAGE_PIPELINE_H
