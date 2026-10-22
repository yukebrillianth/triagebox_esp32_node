/*
 * How much does the BP prediction move when the window is shortened?
 *
 * Not a unit test -- a one-off measurement for the "how short may the window
 * be" question: the Menu offers 30 s or 60 s, and the publish floor
 * (bp_min_samples() in bp_capture.c) must sit where the model still answers.
 * That is measurable rather than arguable.
 *
 * Input is bp_window.csv from `bplog` (tools/bp_capture_csv.py). Its raw_*
 * columns are re-filtered here exactly as bp_capture.c does today (biquads
 * primed on the first sample), NOT read from fil_*: recordings made before the
 * priming fix carry the 6 s start-up transient in fil_*, and every prefix of
 * those is refused. Prefixes of the re-filtered signal are what a shorter
 * measure window feeds bp_predict().
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

/* Same coefficients as bp_capture.c (CMSIS order b0 b1 b2 -a1 -a2). */
static const float k_hp05[5] = {0.978027f, -1.956055f, 0.978027f, 1.955572f, -0.956537f};
static const float k_ecg_bp[5] = {0.230114f, 0.0f, -0.230114f, 1.317384f, -0.539771f};

static void prime(float x0, const float c[5], float st[2])
{
    const float y0 = x0 * (c[0] + c[1] + c[2]) / (1.0f - c[3] - c[4]);
    st[1] = c[2] * x0 + c[4] * y0;
    st[0] = y0 - c[0] * x0;
}

static float biquad(float x, const float c[5], float st[2])
{
    const float y = c[0] * x + st[0];
    st[0] = c[1] * x + c[3] * y + st[1];
    st[1] = c[2] * x + c[4] * y;
    return y;
}

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
        (void) fir; (void) fred; (void) fecg;
        g_ir[n] = (double) rir;
        g_red[n] = (double) rred;
        g_ecg[n] = (double) recg;
        ++n;
    }
    fclose(f);
    printf("%s: %zu samples (%.1f s at %.0f Hz), is_male=%d\n\n", path, n,
           (double) n / BP_SAMPLING_RATE_HZ, BP_SAMPLING_RATE_HZ, is_male);

    {
        float s_ir[2], s_red[2], s_ecg[2];
        prime((float) g_ir[0], k_hp05, s_ir);
        prime((float) g_red[0], k_hp05, s_red);
        prime((float) g_ecg[0], k_ecg_bp, s_ecg);
        for (size_t i = 0; i < n; ++i) {
            g_ir[i] = biquad((float) g_ir[i], k_hp05, s_ir);
            g_red[i] = biquad((float) g_red[i], k_hp05, s_red);
            g_ecg[i] = biquad((float) g_ecg[i], k_ecg_bp, s_ecg);
        }
    }

    /* Prefixes, because that is what a shorter measure window produces: the
     * same first samples, stopped earlier. Sub-windows from the middle would
     * answer a different question (how stationary the signal is). */
    static const double k_secs[] = {60.0, 45.0, 30.0, 29.69, 29.0, 28.0, 27.0, 26.0, 25.0};
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
