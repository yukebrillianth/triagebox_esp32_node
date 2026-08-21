#ifndef PPG_BANDPASS_FILTER_H
#define PPG_BANDPASS_FILTER_H

#include <stddef.h>
#include <stdint.h>

#define PPG_FILTER_ORDER 4
#define PPG_FILTER_FS 100.0f
#define PPG_FILTER_LOWCUT 0.20f
#define PPG_FILTER_HIGHCUT 10.00f
#define PPG_FILTER_STOPBAND_DB 30.0f
#define PPG_SOS_SECTIONS_COUNT 4

static const double PPG_SOS_SECTIONS[4][6] = {
    { 3.4199677780123387e-02, -1.2195553071922327e-02, 3.4199677780123380e-02, 1.0000000000000000e+00, -1.2720142856611345e+00, 4.3096721934027687e-01 },
    { 1.0000000000000000e+00, -1.5618939718626774e+00, 1.0000000000000002e+00, 1.0000000000000000e+00, -1.6341254203972027e+00, 7.9217371967304506e-01 },
    { 1.0000000000000000e+00, -1.9999760931394597e+00, 9.9999999999999978e-01, 1.0000000000000000e+00, -1.9709096327481554e+00, 9.7118903201371742e-01 },
    { 1.0000000000000000e+00, -1.9998644591272929e+00, 9.9999999999999989e-01, 1.0000000000000000e+00, -1.9889606248194938e+00, 9.8932008775676139e-01 }
};

typedef struct {
    double state[PPG_SOS_SECTIONS_COUNT][2];
} ppg_biquad_cascade_t;

static inline void ppg_filter_reset(ppg_biquad_cascade_t *f) {
    for (int s = 0; s < PPG_SOS_SECTIONS_COUNT; s++) {
        f->state[s][0] = 0.0;
        f->state[s][1] = 0.0;
    }
}

static inline double ppg_filter_step(ppg_biquad_cascade_t *f, double input_sample) {
    double x = input_sample;
    for (int s = 0; s < PPG_SOS_SECTIONS_COUNT; s++) {
        double b0 = PPG_SOS_SECTIONS[s][0];
        double b1 = PPG_SOS_SECTIONS[s][1];
        double b2 = PPG_SOS_SECTIONS[s][2];
        double a1 = PPG_SOS_SECTIONS[s][4];
        double a2 = PPG_SOS_SECTIONS[s][5];
        
        double y = b0 * x + f->state[s][0];
        f->state[s][0] = b1 * x - a1 * y + f->state[s][1];
        f->state[s][1] = b2 * x - a2 * y;
        x = y;
    }
    return x;
}
static inline double ppg_normalize_sample(double sample, double min_val, double max_val) {
    double range = max_val - min_val;
    if (range < 1e-5) range = 1e-5;
    double norm = (sample - min_val) / range;
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;
    return norm;
}

void ppg_filter_stream(const double *in, double *out, int n);
void ppg_filtfilt(const double *in, double *out, int n);

#endif /* PPG_BANDPASS_FILTER_H */
