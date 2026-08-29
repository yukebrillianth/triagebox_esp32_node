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
    int candidates[512];
    int cand_count = 0;

    for (size_t i = 1; i < length - 1 && cand_count < 512; i++) {
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

static void compute_morphology_c(
    const double *sig,
    size_t length,
    const int *peaks, int num_peaks,
    const int *valleys, int num_valleys,
    double *pw75_out,
    double *pw50_out,
    double *tsys_out,
    double *tdia_out,
    double *apg_rms_out,
    double *vpg_rms_out
) {
    (void)peaks; (void)num_peaks;
    if (num_valleys < 2) {
        *pw75_out = 120.0;
        *pw50_out = 250.0;
        *tsys_out = 150.0;
        *tdia_out = 650.0;
        *apg_rms_out = 0.05;
        *vpg_rms_out = 0.05;
        return;
    }

    double pw75_sum = 0.0, pw50_sum = 0.0;
    double tsys_sum = 0.0, tdia_sum = 0.0;
    int valid_pulses = 0;

    for (int i = 0; i < num_valleys - 1; i++) {
        int v_start = valleys[i];
        int v_end = valleys[i + 1];
        int pulse_len = v_end - v_start;
        if (pulse_len < 35) continue;

        int p_peak_rel = 0;
        double max_val = sig[v_start];
        for (int k = v_start; k < v_end; k++) {
            if (sig[k] > max_val) {
                max_val = sig[k];
                p_peak_rel = k - v_start;
            }
        }

        tsys_sum += (p_peak_rel / BP_SAMPLING_RATE_HZ) * 1000.0;
        tdia_sum += ((pulse_len - p_peak_rel) / BP_SAMPLING_RATE_HZ) * 1000.0;

        int count_75 = 0, count_50 = 0;
        for (int k = v_start; k < v_end; k++) {
            if (sig[k] >= 0.75) count_75++;
            if (sig[k] >= 0.50) count_50++;
        }
        pw75_sum += (count_75 / BP_SAMPLING_RATE_HZ) * 1000.0;
        pw50_sum += (count_50 / BP_SAMPLING_RATE_HZ) * 1000.0;
        valid_pulses++;
    }

    if (valid_pulses > 0) {
        *pw75_out = pw75_sum / valid_pulses;
        *pw50_out = pw50_sum / valid_pulses;
        *tsys_out = tsys_sum / valid_pulses;
        *tdia_out = tdia_sum / valid_pulses;
    } else {
        *pw75_out = 120.0;
        *pw50_out = 250.0;
        *tsys_out = 150.0;
        *tdia_out = 650.0;
    }

    double *v_temp = (double*)malloc(length * sizeof(double));
    double *a_temp = (double*)malloc(length * sizeof(double));
    if (v_temp && a_temp) {
        bp_compute_derivatives(sig, v_temp, a_temp, length);
        double v_sum_sq = 0.0, a_sum_sq = 0.0;
        for (size_t i = 0; i < length; i++) {
            v_sum_sq += v_temp[i] * v_temp[i];
            a_sum_sq += a_temp[i] * a_temp[i];
        }
        *vpg_rms_out = sqrt(v_sum_sq / length);
        *apg_rms_out = sqrt(a_sum_sq / length);
        free(v_temp);
        free(a_temp);
    } else {
        *vpg_rms_out = 0.05;
        *apg_rms_out = 0.05;
        if (v_temp) free(v_temp);
        if (a_temp) free(a_temp);
    }
}

bool bp_extract_features(
    const double *bandpass_ppg_red,
    const double *bandpass_ppg_ir,
    const double *bandpass_ecg,
    size_t num_samples,
    double is_male,
    double *features_out
) {
    if (!bandpass_ppg_red || !bandpass_ppg_ir || !bandpass_ecg || !features_out || num_samples < 500) {
        return false;
    }

    double *red_norm = (double*)malloc(num_samples * sizeof(double));
    double *ir_norm  = (double*)malloc(num_samples * sizeof(double));
    double *ecg_norm = (double*)malloc(num_samples * sizeof(double));

    if (!red_norm || !ir_norm || !ecg_norm) {
        if (red_norm) free(red_norm);
        if (ir_norm) free(ir_norm);
        if (ecg_norm) free(ecg_norm);
        return false;
    }

    memcpy(red_norm, bandpass_ppg_red, num_samples * sizeof(double));
    memcpy(ir_norm,  bandpass_ppg_ir,  num_samples * sizeof(double));
    memcpy(ecg_norm, bandpass_ecg,     num_samples * sizeof(double));

    // [1] Min-Max normalize 0.0 to 1.0
    bp_min_max_normalize(red_norm, num_samples);
    bp_min_max_normalize(ir_norm,  num_samples);
    bp_min_max_normalize(ecg_norm, num_samples);

    // [2] Pan-Tompkins QRS Energy Integration
    double *ecg_diff = (double*)malloc(num_samples * sizeof(double));
    double *ecg_qrs  = (double*)malloc(num_samples * sizeof(double));
    if (!ecg_diff || !ecg_qrs) {
        free(red_norm); free(ir_norm); free(ecg_norm);
        if (ecg_diff) free(ecg_diff);
        if (ecg_qrs) free(ecg_qrs);
        return false;
    }

    ecg_diff[0] = ecg_norm[1] - ecg_norm[0];
    for (size_t i = 1; i < num_samples - 1; i++) {
        ecg_diff[i] = (ecg_norm[i + 1] - ecg_norm[i - 1]) / 2.0;
    }
    ecg_diff[num_samples - 1] = ecg_norm[num_samples - 1] - ecg_norm[num_samples - 2];

    for (size_t i = 0; i < num_samples; i++) {
        double win_sum = 0.0;
        int count = 0;
        for (int k = -7; k <= 7; k++) {
            int idx = (int)i + k;
            if (idx >= 0 && idx < (int)num_samples) {
                win_sum += ecg_diff[idx] * ecg_diff[idx];
                count++;
            }
        }
        ecg_qrs[i] = (count > 0) ? (win_sum / count) : 0.0;
    }
    bp_min_max_normalize(ecg_qrs, num_samples);

    // [3] Peak Detection
    int ecg_peaks[512], ir_peaks[512], red_peaks[512];
    int num_ecg = find_peaks_1d(ecg_qrs, num_samples, 35, 0.10, ecg_peaks, 512);
    int num_ir  = find_peaks_1d(ir_norm, num_samples, 35, 0.05, ir_peaks,  512);
    int num_red = find_peaks_1d(red_norm,num_samples, 35, 0.05, red_peaks, 512);

    free(ecg_diff);
    free(ecg_qrs);

    if (num_ecg < 3 || num_ir < 3 || num_red < 3) {
        free(red_norm); free(ir_norm); free(ecg_norm);
        return false;
    }

    double rr_sec = (double)(ecg_peaks[num_ecg - 1] - ecg_peaks[0]) / ((num_ecg - 1) * BP_SAMPLING_RATE_HZ);
    if (rr_sec < 0.3) rr_sec = 0.8;
    double pep_est = 60.0 + 0.12 * (1000.0 * rr_sec) * 0.1;

    double *v_ir  = (double*)malloc(num_samples * sizeof(double));
    double *a_ir  = (double*)malloc(num_samples * sizeof(double));
    double *v_red = (double*)malloc(num_samples * sizeof(double));
    double *a_red = (double*)malloc(num_samples * sizeof(double));
    if (!v_ir || !a_ir || !v_red || !a_red) {
        free(red_norm); free(ir_norm); free(ecg_norm);
        /* No `if (p)` guards: free(NULL) is a no-op by definition, and two
         * `if`s on one line is what -Werror=misleading-indentation rejected. */
        free(v_ir); free(a_ir);
        free(v_red); free(a_red);
        return false;
    }
    bp_compute_derivatives(ir_norm, v_ir, a_ir, num_samples);
    bp_compute_derivatives(red_norm, v_red, a_red, num_samples);

    double pat_p_sum = 0.0, pat_f_sum = 0.0, pat_d_sum = 0.0;
    int pat_count = 0;

    for (int e = 0; e < num_ecg; e++) {
        int r_peak = ecg_peaks[e];
        int p_ir_cand = -1;
        for (int p = 0; p < num_ir; p++) {
            if (ir_peaks[p] > r_peak && ir_peaks[p] < r_peak + 60) {
                p_ir_cand = ir_peaks[p];
                break;
            }
        }
        if (p_ir_cand > 0) {
            double del_p = (p_ir_cand - r_peak) / BP_SAMPLING_RATE_HZ * 1000.0;
            int s_start = (p_ir_cand >= 30) ? (p_ir_cand - 30) : 0;
            int f_ir_cand = s_start;
            double min_val = ir_norm[s_start];
            for (int k = s_start; k < p_ir_cand; k++) {
                if (ir_norm[k] < min_val) {
                    min_val = ir_norm[k];
                    f_ir_cand = k;
                }
            }
            double del_f = (f_ir_cand - r_peak) / BP_SAMPLING_RATE_HZ * 1000.0;
            double del_d = 0.0;
            if (f_ir_cand < p_ir_cand) {
                int d_ir_cand = f_ir_cand;
                double max_v = v_ir[f_ir_cand];
                for (int k = f_ir_cand; k < p_ir_cand; k++) {
                    if (v_ir[k] > max_v) {
                        max_v = v_ir[k];
                        d_ir_cand = k;
                    }
                }
                del_d = (d_ir_cand - r_peak) / BP_SAMPLING_RATE_HZ * 1000.0;
            } else {
                del_d = (del_f + del_p) / 2.0;
            }
            pat_p_sum += del_p;
            pat_f_sum += del_f;
            pat_d_sum += del_d;
            pat_count++;
        }
    }

    if (pat_count == 0) {
        free(red_norm); free(ir_norm); free(ecg_norm);
        free(v_ir); free(a_ir); free(v_red); free(a_red);
        return false;
    }

    double pat_p = pat_p_sum / pat_count;
    double pat_f = pat_f_sum / pat_count;
    double pat_d = pat_d_sum / pat_count;

    double pat_f_fridericia = pat_f / (pow(rr_sec, 1.0/3.0) + 1e-5);
    double pat_f_framingham = pat_f + 0.154 * (1.0 - rr_sec) * 1000.0;
    double pat_d_framingham = pat_d + 0.154 * (1.0 - rr_sec) * 1000.0;

    double pat_d_inv    = 1.0 / (pat_d + 1e-5);
    double pat_d_sq_inv = 1.0 / (pat_d * pat_d + 1e-5);
    double pat_p_sq_inv = 1.0 / (pat_p * pat_p + 1e-5);

    double ptt_p_est    = pat_p - pep_est;
    double ptt_f_est    = pat_f - pep_est;
    double ptt_f_sq_inv = 1.0 / (ptt_f_est * ptt_f_est + 1e-5);
    double ptt_d_sq_inv = 1.0 / ((pat_d - pep_est) * (pat_d - pep_est) + 1e-5);
    double ptt_p_sq_inv = 1.0 / (ptt_p_est * ptt_p_est + 1e-5);

    // Valleys
    double *neg_ir  = (double*)malloc(num_samples * sizeof(double));
    double *neg_red = (double*)malloc(num_samples * sizeof(double));
    for (size_t i = 0; i < num_samples; i++) {
        neg_ir[i] = -ir_norm[i];
        neg_red[i]= -red_norm[i];
    }
    int ir_valleys[512], red_valleys[512];
    int num_ir_v  = find_peaks_1d(neg_ir,  num_samples, 35, 0.02, ir_valleys,  512);
    int num_red_v = find_peaks_1d(neg_red, num_samples, 35, 0.02, red_valleys, 512);
    free(neg_ir);
    free(neg_red);

    double pw75, dummy_pw50, tsys, tdia, ir_apg_rms, ir_vpg_rms;
    compute_morphology_c(ir_norm, num_samples, ir_peaks, num_ir, ir_valleys, num_ir_v, &pw75, &dummy_pw50, &tsys, &tdia, &ir_apg_rms, &ir_vpg_rms);

    double red_pw75, red_pw50, red_tsys, red_tdia, red_apg_rms, red_vpg_rms;
    compute_morphology_c(red_norm, num_samples, red_peaks, num_red, red_valleys, num_red_v, &red_pw75, &red_pw50, &red_tsys, &red_tdia, &red_apg_rms, &red_vpg_rms);

    double t_dia_est = (1000.0 * rr_sec) - tsys;
    double k_val = tsys / (t_dia_est + 1e-5);

    double ac_ir = 0.0, ac_red = 0.0, dc_ir = 0.0, dc_red = 0.0;
    double min_raw_i = bandpass_ppg_ir[0], max_raw_i = bandpass_ppg_ir[0], sum_raw_i = 0.0;
    double min_raw_r = bandpass_ppg_red[0], max_raw_r = bandpass_ppg_red[0], sum_raw_r = 0.0;

    for (size_t i = 0; i < num_samples; i++) {
        if (bandpass_ppg_ir[i] < min_raw_i) min_raw_i = bandpass_ppg_ir[i];
        if (bandpass_ppg_ir[i] > max_raw_i) max_raw_i = bandpass_ppg_ir[i];
        sum_raw_i += bandpass_ppg_ir[i];

        if (bandpass_ppg_red[i] < min_raw_r) min_raw_r = bandpass_ppg_red[i];
        if (bandpass_ppg_red[i] > max_raw_r) max_raw_r = bandpass_ppg_red[i];
        sum_raw_r += bandpass_ppg_red[i];
    }
    ac_ir = max_raw_i - min_raw_i;
    dc_ir = (sum_raw_i / num_samples) + 1e-5;
    ac_red = max_raw_r - min_raw_r;
    dc_red = (sum_raw_r / num_samples) + 1e-5;

    double pi_ir = (ac_ir / (fabs(dc_ir) + 1e-5)) * 100.0;
    double ac_dc_ratio = (ac_red + ac_ir) / (dc_red + dc_ir + 1e-5);

    free(red_norm); free(ir_norm); free(ecg_norm);
    free(v_ir); free(a_ir); free(v_red); free(a_red);

    // Populate the 23 dedicated features matching FEAT_* defines
    features_out[FEAT_PAT_D]            = pat_d;
    features_out[FEAT_PAT_P]            = pat_p;
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
    if (!result_sbp) return false;

    double features[NUM_INPUT_FEATURES];
    bool ok = bp_extract_features(bandpass_ppg_red, bandpass_ppg_ir, bandpass_ecg, num_samples, is_male, features);
    if (!ok) return false;

    *result_sbp = predict_sbp(features);

    return true;
}
