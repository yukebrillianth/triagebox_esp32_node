/*
 * 20 rows of the 4-class model's holdout test set (apt2736/PKM_RF, 12,346 rows),
 * first 20 in split order: age, sex (1 = M), systolic BP, HR, RR, SpO2.
 * Real inputs rather than one synthetic patient, so the trees do not all take the
 * same path and the branch predictor does not flatter the timing.
 */
#ifndef TB_BENCH_ROWS_H
#define TB_BENCH_ROWS_H

static const float k_bench_rows[20][6] = {
    {18.0f, 0.0f, 128.0f, 85.0f, 18.0f, 96.5f},
    {38.0f, 0.0f, 129.0f, 100.0f, 19.0f, 96.1f},
    {82.0f, 0.0f, 138.0f, 82.0f, 17.0f, 97.0f},
    {42.0f, 0.0f, 129.0f, 67.0f, 17.0f, 98.8f},
    {39.0f, 1.0f, 156.0f, 121.0f, 23.0f, 91.9f},
    {36.0f, 1.0f, 115.0f, 90.0f, 17.0f, 98.4f},
    {62.0f, 0.0f, 144.0f, 79.0f, 16.0f, 99.3f},
    {34.0f, 1.0f, 142.0f, 86.0f, 20.0f, 97.8f},
    {43.0f, 0.0f, 110.0f, 98.0f, 16.0f, 100.0f},
    {63.0f, 0.0f, 176.0f, 76.0f, 14.0f, 95.2f},
    {58.0f, 0.0f, 131.0f, 93.0f, 19.0f, 93.1f},
    {99.0f, 0.0f, 119.0f, 100.0f, 18.0f, 95.9f},
    {84.0f, 1.0f, 92.0f, 72.0f, 17.0f, 93.2f},
    {45.0f, 0.0f, 112.0f, 107.0f, 17.0f, 94.0f},
    {28.0f, 1.0f, 114.0f, 82.0f, 13.0f, 94.9f},
    {73.0f, 1.0f, 124.0f, 91.0f, 18.0f, 98.7f},
    {62.0f, 1.0f, 143.0f, 92.0f, 20.0f, 97.1f},
    {55.0f, 1.0f, 126.0f, 93.0f, 20.0f, 92.4f},
    {49.0f, 1.0f, 144.0f, 117.0f, 27.0f, 91.1f},
    {85.0f, 0.0f, 117.0f, 80.0f, 17.0f, 100.0f},
};

#endif
