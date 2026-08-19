#ifndef ECG_FILTER_H
#define ECG_FILTER_H

#include <stddef.h>
#include <stdint.h>

#define ECG_BP_ORDER 3
#define ECG_FILTER_FS 100.0f
#define ECG_BP_LOWCUT 0.50f
#define ECG_BP_HIGHCUT 35.00f
#define ECG_BP_SOS_SECTIONS_COUNT 3
#define ECG_PAN_TOMPKINS_WINDOW_LEN 12

/* ECG Bandpass SOS Matrix [b0, b1, b2, a0, a1, a2] @ 100.0 Hz */
static const double ECG_BP_SOS[3][6] = {
    { 3.6126366397632920e-01, 7.2252732795265839e-01, 3.6126366397632920e-01, 1.0000000000000000e+00, 8.3569339116082209e-01, 4.2801678128462939e-01 },
    { 1.0000000000000000e+00, 0.0000000000000000e+00, -1.0000000000000000e+00, 1.0000000000000000e+00, -6.5094422222856663e-01, -3.0764016965989832e-01 },
    { 1.0000000000000000e+00, -2.0000000000000000e+00, 1.0000000000000000e+00, 1.0000000000000000e+00, -1.9683451210140963e+00, 9.6932451350981452e-01 }
};

typedef struct {
    double bp_state[ECG_BP_SOS_SECTIONS_COUNT][2];
    double prev_sample;
    double window_buffer[ECG_PAN_TOMPKINS_WINDOW_LEN];
    int buffer_idx;
    double window_sum;
} ecg_filter_state_t;

static inline void ecg_filter_reset(ecg_filter_state_t *f) {
    for (int s = 0; s < ECG_BP_SOS_SECTIONS_COUNT; s++) {
        f->bp_state[s][0] = 0.0;
        f->bp_state[s][1] = 0.0;
    }
    f->prev_sample = 0.0;
    f->buffer_idx = 0;
    f->window_sum = 0.0;
    for (int i = 0; i < ECG_PAN_TOMPKINS_WINDOW_LEN; i++) {
        f->window_buffer[i] = 0.0;
    }
}

/* Step function for BandPass Filter */
static inline double ecg_filter_step(ecg_filter_state_t *f, double input_sample) {
    double x = input_sample;
    for (int s = 0; s < ECG_BP_SOS_SECTIONS_COUNT; s++) {
        double b0 = ECG_BP_SOS[s][0], b1 = ECG_BP_SOS[s][1], b2 = ECG_BP_SOS[s][2];
        double a1 = ECG_BP_SOS[s][4], a2 = ECG_BP_SOS[s][5];
        double y = b0 * x + f->bp_state[s][0];
        f->bp_state[s][0] = b1 * x - a1 * y + f->bp_state[s][1];
        f->bp_state[s][1] = b2 * x - a2 * y;
        x = y;
    }
    return x;
}

/* Step function for Real-Time Pan-Tompkins QRS Energy Envelope */
static inline double ecg_pan_tompkins_step(ecg_filter_state_t *f, double filtered_sample) {
    double diff = filtered_sample - f->prev_sample;
    f->prev_sample = filtered_sample;
    double squared = diff * diff;
    
    // Update circular moving average window
    f->window_sum -= f->window_buffer[f->buffer_idx];
    f->window_buffer[f->buffer_idx] = squared;
    f->window_sum += squared;
    f->buffer_idx = (f->buffer_idx + 1) % ECG_PAN_TOMPKINS_WINDOW_LEN;
    
    return f->window_sum / ECG_PAN_TOMPKINS_WINDOW_LEN;
}

#endif /* ECG_FILTER_H */
