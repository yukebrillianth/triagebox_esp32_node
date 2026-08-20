#include "ecg_filter.h"
#include <stdlib.h>
#include <string.h>

void ecg_filter_stream(const double *in, double *out, int n) {
    ecg_filter_state_t filter;
    ecg_filter_reset(&filter);
    for (int i = 0; i < n; i++) {
        out[i] = ecg_filter_step(&filter, in[i]);
    }
}

void ecg_filtfilt(const double *in, double *out, int n) {
    if (n <= 0) return;
    double *fwd = (double*)malloc(n * sizeof(double));
    if (!fwd) return;

    double mean_v = 0.0;
    for (int i = 0; i < n; i++) {
        mean_v += in[i];
    }
    mean_v /= (double)n;

    ecg_filter_state_t filter;
    ecg_filter_reset(&filter);

    // Forward pass (demeaned)
    for (int i = 0; i < n; i++) {
        fwd[i] = ecg_filter_step(&filter, in[i] - mean_v);
    }

    // Backward pass
    ecg_filter_reset(&filter);
    for (int i = n - 1; i >= 0; i--) {
        out[i] = ecg_filter_step(&filter, fwd[i]);
    }

    free(fwd);
}
