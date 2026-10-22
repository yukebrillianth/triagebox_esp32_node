/*
 * How much does the BP prediction move when the window is shortened?
 *
 * Not a unit test -- a one-off measurement for the "can the measurement be made
 * faster" question. UI_MEASURE_MS is 60 s and BP_MIN_SAMPLES is 4800, so the
 * window COULD close at 48 s; whether that is honest depends on how much the
 * model's answer depends on window length, and that is measurable rather than
 * arguable.
 *
 * Input is bp_window.csv from `bplog` (tools/bp_capture_csv.py): its fil_ir /
 * fil_red / fil_ecg columns are exactly what the device fed bp_predict(), so
 * prefixes of them are exactly what a shorter UI_MEASURE_MS would have fed it.
 *
 * Build (from the repo root):
 *   cc -std=c99 -O1 -I components/triagebox_ml/include \
 *      -o /tmp/bp_window_sweep tools/bp_window_sweep.c \
 *      components/triagebox_ml/bp_pipeline.c \
 *      components/triagebox_ml/lgbm_sbp.c components/triagebox_ml/lgbm_dbp.c -lm
 *   /tmp/bp_window_sweep bp_window.csv
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bp_pipeline.h"

#define MAX_N 6000

static double g_ir[MAX_N], g_red[MAX_N], g_ecg[MAX_N];

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "bp_window.csv";
    FILE *f = fopen(path, "r");
    char line[512];
    size_t n = 0;
    int is_male = 1;

    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    while (fgets(line, sizeof(line), f) != NULL && n < MAX_N) {
        if (line[0] == '#') {
            const char *m = strstr(line, "is_male=");
            if (m != NULL) {
                is_male = atoi(m + 8);
            }
            continue;
        }
        if (line[0] == 'i') {
            continue; /* header row */
        }
        /* i,raw_ir,raw_red,raw_ecg,fil_ir,fil_red,fil_ecg */
        unsigned i, rir, rred;
        int recg;
        double fir, fred, fecg;
        if (sscanf(line, "%u,%u,%u,%d,%lf,%lf,%lf", &i, &rir, &rred, &recg,
                   &fir, &fred, &fecg) != 7) {
            continue;
        }
        g_ir[n] = fir;
        g_red[n] = fred;
        g_ecg[n] = fecg;
        ++n;
    }
    fclose(f);
    printf("%s: %zu samples (%.1f s at %.0f Hz), is_male=%d\n\n", path, n,
           (double) n / BP_SAMPLING_RATE_HZ, BP_SAMPLING_RATE_HZ, is_male);

    /* Prefixes, because that is what a shorter measure window produces: the
     * same first samples, stopped earlier. Sub-windows from the middle would
     * answer a different question (how stationary the signal is). */
    static const double k_secs[] = {60.0, 55.0, 50.0, 48.0, 45.0, 40.0, 30.0};
    double base_sbp = 0.0, base_dbp = 0.0;
    int have_base = 0;

    for (unsigned k = 0; k < sizeof(k_secs) / sizeof(k_secs[0]); ++k) {
        size_t want = (size_t) (k_secs[k] * BP_SAMPLING_RATE_HZ);
        double sbp = 0.0, dbp = 0.0;

        if (want > n) {
            want = n;
        }
        /* bp_pipeline takes (red, ir, ecg) -- red first, see bp_pipeline.h. */
        if (!bp_predict(g_red, g_ir, g_ecg, want, (double) is_male, &sbp, &dbp)) {
            printf("  %4.0f s (%4zu samples): REFUSED\n", k_secs[k], want);
            continue;
        }
        if (!have_base) {
            base_sbp = sbp;
            base_dbp = dbp;
            have_base = 1;
            printf("  %4.0f s (%4zu samples): %6.1f / %5.1f   (reference)\n",
                   k_secs[k], want, sbp, dbp);
        } else {
            printf("  %4.0f s (%4zu samples): %6.1f / %5.1f   "
                   "delta %+6.1f / %+5.1f mmHg\n",
                   k_secs[k], want, sbp, dbp, sbp - base_sbp, dbp - base_dbp);
        }
    }
    return 0;
}
