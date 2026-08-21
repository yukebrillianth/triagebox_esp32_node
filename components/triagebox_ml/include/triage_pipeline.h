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
    brief: Setelah 1 menit pertama setelah tekan tombol 'START', yang dipake buat
    prediksi triase yang ini.
    ret: ESI 1..5, atau 0 kalau ada sensor yang tidak melapor (min < 1).

    CATATAN: nilai balik = ESI mentah, BUKAN ui_priority_t. Konversinya wajib
    lewat tb_triage_esi_to_priority() di tb_triage.h -- cast langsung memetakan
    ESI 1 (paling kritis) ke YELLOW.
*/
int predict_triage_start_init(const TriageInput* input);

/*
    brief: Setelah eksekusi predict_triage_start_init(), ini yang dipake buat
    update kondisi pasien selama interval tertentu (15 detik?). Menyimpan state
    smoothing internal, jadi urutan pemanggilan berarti.
    ret: ESI 1..5, atau 0. Lihat catatan konversi di atas.
*/
int predict_triage_start_continue(const TriageInput* input, int reset_state);

#ifdef __cplusplus
}
#endif
#endif // TRIAGE_PIPELINE_H
