#ifndef TRIAGE_PIPELINE_H
#define TRIAGE_PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float age;              
    float sex;              // 1.0 = Male, 0.0 = Female
    float systolic_bp;      // Systolic blood pressure (mmHg)
    float heart_rate;       // Heart rate (beats/min)
    float respiratory_rate; // Respiratory rate (breaths/min)
    float spo2;             // Oxygen saturation (%)
} TriageInput;

typedef struct {
    float probs[5];         
    int predicted_esi;      
} TriageOutput;

TriageOutput predict_triage(const TriageInput* input);

#ifdef __cplusplus
}
#endif

#endif // TRIAGE_PIPELINE_H
