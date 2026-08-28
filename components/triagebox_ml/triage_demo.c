#include "tb_classify.h"

// Contoh penggunaan model

void demo_triage() {
    TriageInput input;
    input.age = 30.0f; // Ambil median dri kelompok usia
    input.sex = 1.0f; 
    input.systolic_bp = 120.0f;
    input.heart_rate = 80.0f;
    input.respiratory_rate = 16.0f;
    input.spo2 = 98.0f;
    input.airway_problem = 

    int predicted_esi;
    ui_priority_t priority = tb_classify(&input, &predicted_esi);
}