#include "bp_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void bp_min_max_normalize(double *buffer, size_t length) {
    if (length == 0) return;
    double min_v = buffer[0];
    double max_v = buffer[0];
    for (size_t i = 1; i < length; i++) {
        if (buffer[i] < min_v) min_v = buffer[i];
        if (buffer[i] > max_v) max_v = buffer[i];
    }
    double range = max_v - min_v;
    if (range > 1e-9) {
        for (size_t i = 0; i < length; i++) {
            buffer[i] = (buffer[i] - min_v) / (range + 1e-5);
        }
    } else {
        for (size_t i = 0; i < length; i++) {
            buffer[i] = 0.0;
        }
    }
}

void bp_compute_derivatives(const double *input, double *v_out, double *a_out, size_t length) {
    if (length < 3) return;
    v_out[0] = input[1] - input[0];
    for (size_t i = 1; i < length - 1; i++) {
        v_out[i] = (input[i + 1] - input[i - 1]) / 2.0;
    }
    v_out[length - 1] = input[length - 1] - input[length - 2];

    a_out[0] = v_out[1] - v_out[0];
    for (size_t i = 1; i < length - 1; i++) {
        a_out[i] = (v_out[i + 1] - v_out[i - 1]) / 2.0;
    }
    a_out[length - 1] = v_out[length - 1] - v_out[length - 2];
}

static int find_peaks_1d(const double *sig, size_t length, int min_dist, double min_prom, int *peaks_out, int max_peaks) {
    int candidates[256];
    int cand_count = 0;

    for (size_t i = 1; i < length - 1 && cand_count < 256; i++) {
        if (sig[i] > sig[i - 1] && sig[i] >= sig[i + 1]) {
            double left_min = sig[i];
            for (int k = (int)i - 1; k >= 0; k--) {
                if (sig[k] > sig[i]) break;
                if (sig[k] < left_min) left_min = sig[k];
            }
            double right_min = sig[i];
            for (size_t k = i + 1; k < length; k++) {
                if (sig[k] > sig[i]) break;
                if (sig[k] < right_min) right_min = sig[k];
            }
            double prom = sig[i] - ((left_min > right_min) ? left_min : right_min);
            if (prom >= min_prom) {
                candidates[cand_count++] = (int)i;
            }
        }
    }

    int peak_count = 0;
    for (int i = 0; i < cand_count && peak_count < max_peaks; i++) {
        int curr = candidates[i];
        bool keep = true;
        for (int j = 0; j < peak_count; j++) {
            if (abs(curr - peaks_out[j]) < min_dist) {
                if (sig[curr] > sig[peaks_out[j]]) {
                    peaks_out[j] = curr;
                }
                keep = false;
                break;
            }
        }
        if (keep) {
            peaks_out[peak_count++] = curr;
        }
    }

    for (int i = 0; i < peak_count - 1; i++) {
        for (int j = i + 1; j < peak_count; j++) {
            if (peaks_out[i] > peaks_out[j]) {
                int temp = peaks_out[i];
                peaks_out[i] = peaks_out[j];
                peaks_out[j] = temp;
            }
        }
    }
    return peak_count;
}

static int find_valleys_1d(const double *sig, size_t length, int min_dist, int *valleys_out, int max_valleys) {
    double inv[1024];
    size_t n = (length > 1024) ? 1024 : length;
    for (size_t i = 0; i < n; i++) {
        inv[i] = -sig[i];
    }
    return find_peaks_1d(inv, n, min_dist, 0.02, valleys_out, max_valleys);
}

const char *bp_get_aha_classification(double sbp) {
    if (sbp < 120.0) {
        return "Normal Blood Pressure";
    } else if (sbp < 130.0) {
        return "Elevated Blood Pressure";
    } else if (sbp < 140.0) {
        return "Stage 1 Hypertension";
    } else if (sbp < 180.0) {
        return "Stage 2 Hypertension";
    } else {
        return "Hypertensive Crisis (Seek Immediate Medical Care)";
    }
}

bool bp_extract_features(
    const double *bandpass_ppg_red,
    const double *bandpass_ppg_ir,
    const double *bandpass_ecg,
    size_t num_samples,
    double is_male,
    double features_out[NUM_INPUT_FEATURES]
) {
    if (num_samples < 200) return false;
    size_t n = (num_samples > BP_WINDOW_SAMPLES) ? BP_WINDOW_SAMPLES : num_samples;

    double red_norm[BP_WINDOW_SAMPLES];
    double ir_norm[BP_WINDOW_SAMPLES];
    double ecg_norm[BP_WINDOW_SAMPLES];

    for (size_t i = 0; i < n; i++) {
        red_norm[i] = bandpass_ppg_red[i];
        ir_norm[i]  = bandpass_ppg_ir[i];
        ecg_norm[i] = bandpass_ecg[i];
    }
    bp_min_max_normalize(red_norm, n);
    bp_min_max_normalize(ir_norm, n);
    bp_min_max_normalize(ecg_norm, n);

    double ecg_diff[BP_WINDOW_SAMPLES];
    double ecg_sq[BP_WINDOW_SAMPLES];
    double ecg_qrs[BP_WINDOW_SAMPLES];

    ecg_diff[0] = ecg_norm[1] - ecg_norm[0];
    for (size_t i = 1; i < n - 1; i++) {
        ecg_diff[i] = (ecg_norm[i + 1] - ecg_norm[i - 1]) / 2.0;
    }
    ecg_diff[n - 1] = ecg_norm[n - 1] - ecg_norm[n - 2];

    for (size_t i = 0; i < n; i++) {
        ecg_sq[i] = ecg_diff[i] * ecg_diff[i];
    }

    for (size_t i = 0; i < n; i++) {
        double sum = 0.0;
        int count = 0;
        for (int k = -7; k <= 7; k++) {
            int idx = (int)i + k;
            if (idx >= 0 && idx < (int)n) {
                sum += ecg_sq[idx];
                count++;
            }
        }
        ecg_qrs[i] = sum / (double)count;
    }
    bp_min_max_normalize(ecg_qrs, n);

    int ecg_peaks[64];
    int ir_peaks[64];
    int red_peaks[64];

    int num_ecg = find_peaks_1d(ecg_qrs, n, 35, 0.10, ecg_peaks, 64);
    int num_ir  = find_peaks_1d(ir_norm,  n, 35, 0.05, ir_peaks, 64);
    int num_red = find_peaks_1d(red_norm, n, 35, 0.05, red_peaks, 64);

    if (num_ecg < 3 || num_ir < 3 || num_red < 3) {
        return false;
    }

    double rr_sec = (double)(ecg_peaks[num_ecg - 1] - ecg_peaks[0]) / ((num_ecg - 1) * BP_SAMPLING_RATE_HZ);
    if (rr_sec < 0.3) rr_sec = 0.8;
    double pep_est = 60.0 + 0.12 * (1000.0 * rr_sec) * 0.1;

    double v_ir[BP_WINDOW_SAMPLES], a_ir[BP_WINDOW_SAMPLES];
    double v_red[BP_WINDOW_SAMPLES], a_red[BP_WINDOW_SAMPLES];
    bp_compute_derivatives(ir_norm, v_ir, a_ir, n);
    bp_compute_derivatives(red_norm, v_red, a_red, n);

    double pat_p_vals[64], pat_f_vals[64], pat_d_vals[64];
    int valid_cycles = 0;

    for (int e = 0; e < num_ecg; e++) {
        int r_i = ecg_peaks[e];
        int p_ir = -1;
        for (int p = 0; p < num_ir; p++) {
            if (ir_peaks[p] > r_i && ir_peaks[p] < r_i + 60) {
                p_ir = ir_peaks[p];
                break;
            }
        }
        if (p_ir < 0) continue;

        int s_ir = (p_ir - 30 >= 0) ? p_ir - 30 : 0;
        int f_ir = s_ir;
        double min_f = ir_norm[s_ir];
        for (int k = s_ir + 1; k < p_ir; k++) {
            if (ir_norm[k] < min_f) {
                min_f = ir_norm[k];
                f_ir = k;
            }
        }

        int d_ir = f_ir;
        if (f_ir < p_ir) {
            double max_v = v_ir[f_ir];
            for (int k = f_ir + 1; k < p_ir; k++) {
                if (v_ir[k] > max_v) {
                    max_v = v_ir[k];
                    d_ir = k;
                }
            }
        }

        double del_p = (double)(p_ir - r_i) * 10.0;
        double del_f = (double)(f_ir - r_i) * 10.0;
        double del_d = (double)(d_ir - r_i) * 10.0;

        pat_p_vals[valid_cycles] = del_p;
        pat_f_vals[valid_cycles] = del_f;
        pat_d_vals[valid_cycles] = del_d;
        valid_cycles++;
    }

    if (valid_cycles == 0) return false;

    double sum_pat_p = 0.0, sum_pat_f = 0.0, sum_pat_d = 0.0;
    for (int i = 0; i < valid_cycles; i++) {
        sum_pat_p += pat_p_vals[i];
        sum_pat_f += pat_f_vals[i];
        sum_pat_d += pat_d_vals[i];
    }
    double pat_p_mean = sum_pat_p / valid_cycles;
    double pat_f_mean = sum_pat_f / valid_cycles;
    double pat_d_mean = sum_pat_d / valid_cycles;

    double pat_f_fridericia = pat_f_mean / (cbrt(rr_sec) + 1e-5);
    double pat_f_framingham = pat_f_mean + 0.154 * (1.0 - rr_sec) * 1000.0;
    double pat_d_framingham = pat_d_mean + 0.154 * (1.0 - rr_sec) * 1000.0;

    double pat_d_inv    = 1.0 / (pat_d_mean + 1e-5);
    double pat_d_sq_inv = 1.0 / (pat_d_mean * pat_d_mean + 1e-5);
    double pat_p_sq_inv = 1.0 / (pat_p_mean * pat_p_mean + 1e-5);

    double ptt_p_est    = pat_p_mean - pep_est;
    double ptt_f_est    = pat_f_mean - pep_est;
    double ptt_d_est    = pat_d_mean - pep_est;

    double ptt_f_sq_inv = 1.0 / (ptt_f_est * ptt_f_est + 1e-5);
    double ptt_d_sq_inv = 1.0 / (ptt_d_est * ptt_d_est + 1e-5);
    double ptt_p_sq_inv = 1.0 / (ptt_p_est * ptt_p_est + 1e-5);

    int ir_feet[64], red_feet[64];
    int num_ir_feet  = find_valleys_1d(ir_norm, n, 35, ir_feet, 64);
    int num_red_feet = find_valleys_1d(red_norm, n, 35, red_feet, 64);

    double sum_pw75 = 0.0, sum_tsys = 0.0;
    int cnt_ir_pulses = 0;
    for (int i = 0; i < num_ir_feet - 1; i++) {
        int start = ir_feet[i], end = ir_feet[i + 1];
        if (end - start >= 35) {
            int c75 = 0, p_pk = 0;
            double mx = ir_norm[start];
            for (int k = start; k < end; k++) {
                if (ir_norm[k] >= 0.75) c75++;
                if (ir_norm[k] > mx) { mx = ir_norm[k]; p_pk = k - start; }
            }
            sum_pw75 += c75 * 10.0;
            sum_tsys += p_pk * 10.0;
            cnt_ir_pulses++;
        }
    }
    double pw75 = (cnt_ir_pulses > 0) ? sum_pw75 / cnt_ir_pulses : 150.0;
    double tsys = (cnt_ir_pulses > 0) ? sum_tsys / cnt_ir_pulses : 180.0;

    double sum_red_pw50 = 0.0;
    int cnt_red_pulses = 0;
    for (int i = 0; i < num_red_feet - 1; i++) {
        int start = red_feet[i], end = red_feet[i + 1];
        if (end - start >= 35) {
            int c50 = 0;
            for (int k = start; k < end; k++) {
                if (red_norm[k] >= 0.50) c50++;
            }
            sum_red_pw50 += c50 * 10.0;
            cnt_red_pulses++;
        }
    }
    double red_pw50 = (cnt_red_pulses > 0) ? sum_red_pw50 / cnt_red_pulses : 300.0;

    double t_dia_est = (1000.0 * rr_sec) - tsys;
    double k_val = tsys / (t_dia_est + 1e-5);

    double min_raw_ir = bandpass_ppg_ir[0], max_raw_ir = bandpass_ppg_ir[0], sum_raw_ir = 0.0;
    double min_raw_red = bandpass_ppg_red[0], max_raw_red = bandpass_ppg_red[0], sum_raw_red = 0.0;
    double sum_sq_vir = 0.0, sum_sq_air = 0.0, sum_sq_vred = 0.0;

    for (size_t i = 0; i < n; i++) {
        if (bandpass_ppg_ir[i] < min_raw_ir) min_raw_ir = bandpass_ppg_ir[i];
        if (bandpass_ppg_ir[i] > max_raw_ir) max_raw_ir = bandpass_ppg_ir[i];
        sum_raw_ir += bandpass_ppg_ir[i];

        if (bandpass_ppg_red[i] < min_raw_red) min_raw_red = bandpass_ppg_red[i];
        if (bandpass_ppg_red[i] > max_raw_red) max_raw_red = bandpass_ppg_red[i];
        sum_raw_red += bandpass_ppg_red[i];

        sum_sq_vir += v_ir[i] * v_ir[i];
        sum_sq_air += a_ir[i] * a_ir[i];
        sum_sq_vred += v_red[i] * v_red[i];
    }

    double ac_ir = max_raw_ir - min_raw_ir;
    double dc_ir = (sum_raw_ir / (double)n) + 1e-5;
    double pi_ir = (ac_ir / (fabs(dc_ir) + 1e-5)) * 100.0;

    double ac_red = max_raw_red - min_raw_red;
    double dc_red = (sum_raw_red / (double)n) + 1e-5;
    double ac_dc_ratio = (ac_red + ac_ir) / (dc_red + dc_ir + 1e-5);

    double ir_vpg_rms  = sqrt(sum_sq_vir / (double)n);
    double ir_apg_rms  = sqrt(sum_sq_air / (double)n);
    double red_vpg_rms = sqrt(sum_sq_vred / (double)n);

    features_out[FEAT_PAT_D]            = pat_d_mean;
    features_out[FEAT_PAT_P]            = pat_p_mean;
    features_out[FEAT_PAT_F_FRIDERICIA] = pat_f_fridericia;
    features_out[FEAT_PAT_F_FRAMINGHAM] = pat_f_framingham;
    features_out[FEAT_PAT_D_FRAMINGHAM] = pat_d_framingham;
    features_out[FEAT_PAT_D_INV]        = pat_d_inv;
    features_out[FEAT_PAT_D_SQ_INV]     = pat_d_sq_inv;
    features_out[FEAT_PAT_P_SQ_INV]     = pat_p_sq_inv;
    features_out[FEAT_PTT_P_EST]        = ptt_p_est;
    features_out[FEAT_PTT_F_EST]        = ptt_f_est;
    features_out[FEAT_PTT_F_SQ_INV]     = ptt_f_sq_inv;
    features_out[FEAT_PTT_D_SQ_INV]     = ptt_d_sq_inv;
    features_out[FEAT_PTT_P_SQ_INV]     = ptt_p_sq_inv;
    features_out[FEAT_PW75]             = pw75;
    features_out[FEAT_K_VAL]            = k_val;
    features_out[FEAT_PI_IR]            = pi_ir;
    features_out[FEAT_AC_DC_RATIO]      = ac_dc_ratio;
    features_out[FEAT_IR_VPG_RMS]       = ir_vpg_rms;
    features_out[FEAT_IR_APG_RMS]       = ir_apg_rms;
    features_out[FEAT_RED_VPG_RMS]      = red_vpg_rms;
    features_out[FEAT_IR_TSYS]          = tsys;
    features_out[FEAT_RED_PW50]         = red_pw50;
    features_out[FEAT_SEX]              = is_male;

    return true;
}

bool bp_predict(
    const double *bandpass_ppg_red,
    const double *bandpass_ppg_ir,
    const double *bandpass_ecg,
    size_t num_samples,
    double is_male,
    double *result_sbp
) {
    if (!result) return false;

    double features[NUM_INPUT_FEATURES];
    bool ok = bp_extract_features(bandpass_ppg_red, bandpass_ppg_ir, bandpass_ecg, num_samples, is_male, features);
    if (!ok) return false;

    result->sbp = predict_sbp(features);

    return true;
}
