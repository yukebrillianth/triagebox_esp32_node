#include "bp_pipeline.h"
#include "ppg_bandpass_filter.h"
#include "ecg_filter.h"
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
            buffer[i] = (buffer[i] - min_v) / range;
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

            double higher_valley = (left_min > right_min) ? left_min : right_min;
            double prom = sig[i] - higher_valley;

            if (prom >= min_prom) {
                candidates[cand_count++] = (int)i;
            }
        }
    }

    if (cand_count == 0) return 0;

    int order[256];
    for (int i = 0; i < cand_count; i++) order[i] = i;
    for (int i = 0; i < cand_count - 1; i++) {
        for (int j = i + 1; j < cand_count; j++) {
            if (sig[candidates[order[j]]] > sig[candidates[order[i]]]) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    int kept[256];
    int kept_count = 0;
    for (int i = 0; i < cand_count && kept_count < max_peaks; i++) {
        int c = candidates[order[i]];
        bool keep = true;
        for (int k = 0; k < kept_count; k++) {
            if (abs(c - kept[k]) < min_dist) {
                keep = false;
                break;
            }
        }
        if (keep) {
            kept[kept_count++] = c;
        }
    }

    for (int i = 0; i < kept_count - 1; i++) {
        for (int j = i + 1; j < kept_count; j++) {
            if (kept[i] > kept[j]) {
                int tmp = kept[i];
                kept[i] = kept[j];
                kept[j] = tmp;
            }
        }
    }

    for (int i = 0; i < kept_count; i++) {
        peaks_out[i] = kept[i];
    }
    return kept_count;
}

typedef struct {
    double pw25, pw50, pw75;
    double area_a1, area_a2, area_ratio, ipa_ratio;
    double aix, decay_slope, tsys, tdia;
    double apg_b_a, apg_agi;
} morph_features_t;

static morph_features_t compute_single_pulse_morphology(const double *pulse, size_t pulse_len) {
    morph_features_t m;
    memset(&m, 0, sizeof(m));
    if (pulse_len < 20) return m;

    size_t p_peak = 0;
    double max_p = pulse[0], min_p = pulse[0];
    for (size_t i = 1; i < pulse_len; i++) {
        if (pulse[i] > max_p) {
            max_p = pulse[i];
            p_peak = i;
        }
        if (pulse[i] < min_p) {
            min_p = pulse[i];
        }
    }
    double p_range = max_p - min_p;
    if (p_range < 1e-5) return m;

    m.tsys = ((double)p_peak / BP_SAMPLING_RATE_HZ) * 1000.0;
    m.tdia = (((double)(pulse_len - 1 - p_peak)) / BP_SAMPLING_RATE_HZ) * 1000.0;

    double *p_norm = (double *)malloc(pulse_len * sizeof(double));
    if (!p_norm) return m;
    for (size_t i = 0; i < pulse_len; i++) {
        p_norm[i] = (pulse[i] - min_p) / p_range;
    }

    int c25 = 0, c50 = 0, c75 = 0;
    double sum_a1 = 0.0, sum_a2 = 0.0;
    for (size_t i = 0; i < pulse_len; i++) {
        if (p_norm[i] >= 0.25) c25++;
        if (p_norm[i] >= 0.50) c50++;
        if (p_norm[i] >= 0.75) c75++;
        if (i <= p_peak) sum_a1 += p_norm[i];
        else sum_a2 += p_norm[i];
    }
    m.pw25 = ((double)c25 / BP_SAMPLING_RATE_HZ) * 1000.0;
    m.pw50 = ((double)c50 / BP_SAMPLING_RATE_HZ) * 1000.0;
    m.pw75 = ((double)c75 / BP_SAMPLING_RATE_HZ) * 1000.0;
    m.area_a1 = sum_a1;
    m.area_a2 = sum_a2;
    m.area_ratio = sum_a1 / (sum_a2 + 1e-5);
    m.ipa_ratio = sum_a2 / (sum_a1 + sum_a2 + 1e-5);

    size_t decay_len = pulse_len - 1 - p_peak;
    if (decay_len > 0) {
        m.decay_slope = (p_norm[pulse_len - 1] - p_norm[p_peak]) / ((double)decay_len / BP_SAMPLING_RATE_HZ + 1e-5);
    }

    double *v = (double *)malloc(pulse_len * sizeof(double));
    double *a = (double *)malloc(pulse_len * sizeof(double));
    if (v && a) {
        bp_compute_derivatives(p_norm, v, a, pulse_len);

        int a_peaks[32];
        int n_a_p = find_peaks_1d(a, pulse_len, 3, 0.001, a_peaks, 32);
        double a_val = (n_a_p > 0) ? a[a_peaks[0]] : a[0];
        if (fabs(a_val) < 1e-5) a_val = 1.0;

        double b_val = a[0];
        for (size_t i = 0; i <= p_peak && i < pulse_len; i++) {
            if (a[i] < b_val) b_val = a[i];
        }
        m.apg_b_a = b_val / (fabs(a_val) + 1e-5);
        m.apg_agi = b_val / (fabs(a_val) + 1e-5);

        double *neg_v = (double *)malloc(pulse_len * sizeof(double));
        if (neg_v) {
            for (size_t i = 0; i < pulse_len; i++) neg_v[i] = -v[i];
            int v_valleys[32];
            int n_vv = find_peaks_1d(neg_v, pulse_len, 3, 0.001, v_valleys, 32);
            int notch_idx = -1;
            for (int k = 0; k < n_vv; k++) {
                if ((size_t)v_valleys[k] > p_peak) {
                    notch_idx = v_valleys[k];
                    break;
                }
            }
            if (notch_idx >= 0 && (size_t)notch_idx < pulse_len - 1) {
                size_t dia_peak = (size_t)notch_idx;
                double max_dia = p_norm[dia_peak];
                for (size_t i = (size_t)notch_idx; i < pulse_len; i++) {
                    if (p_norm[i] > max_dia) {
                        max_dia = p_norm[i];
                        dia_peak = i;
                    }
                }
                m.aix = (p_norm[dia_peak] - p_norm[p_peak]) / (p_norm[p_peak] + 1e-5);
            } else {
                m.aix = 0.0;
            }
            free(neg_v);
        }
        free(v); free(a);
    }
    free(p_norm);
    return m;
}

static morph_features_t compute_morphology(const double *sig, size_t length) {
    double *neg_sig = (double *)malloc(length * sizeof(double));
    if (!neg_sig) return compute_single_pulse_morphology(sig, length);
    for (size_t i = 0; i < length; i++) neg_sig[i] = -sig[i];

    int feet[64];
    int n_feet = find_peaks_1d(neg_sig, length, (int)(0.35 * BP_SAMPLING_RATE_HZ), 0.02, feet, 64);
    free(neg_sig);

    if (n_feet < 2) {
        return compute_single_pulse_morphology(sig, length);
    }

    morph_features_t avg;
    memset(&avg, 0, sizeof(avg));
    int valid_count = 0;

    for (int i = 0; i < n_feet - 1; i++) {
        int f_start = feet[i];
        int f_end = feet[i + 1];
        int plen = f_end - f_start;
        if (plen >= 35) {
            morph_features_t single = compute_single_pulse_morphology(sig + f_start, (size_t)plen);
            avg.pw25 += single.pw25;
            avg.pw50 += single.pw50;
            avg.pw75 += single.pw75;
            avg.area_a1 += single.area_a1;
            avg.area_a2 += single.area_a2;
            avg.area_ratio += single.area_ratio;
            avg.ipa_ratio += single.ipa_ratio;
            avg.aix += single.aix;
            avg.decay_slope += single.decay_slope;
            avg.tsys += single.tsys;
            avg.tdia += single.tdia;
            avg.apg_b_a += single.apg_b_a;
            avg.apg_agi += single.apg_agi;
            valid_count++;
        }
    }

    if (valid_count > 0) {
        avg.pw25 /= valid_count;
        avg.pw50 /= valid_count;
        avg.pw75 /= valid_count;
        avg.area_a1 /= valid_count;
        avg.area_a2 /= valid_count;
        avg.area_ratio /= valid_count;
        avg.ipa_ratio /= valid_count;
        avg.aix /= valid_count;
        avg.decay_slope /= valid_count;
        avg.tsys /= valid_count;
        avg.tdia /= valid_count;
        avg.apg_b_a /= valid_count;
        avg.apg_agi /= valid_count;
        return avg;
    }

    return compute_single_pulse_morphology(sig, length);
}

bool bp_extract_features(
    const double *raw_ppg_red,
    const double *raw_ppg_ir,
    const double *raw_ecg,
    size_t num_samples,
    double is_male,
    double features_out[NUM_INPUT_FEATURES]
) {
    if (num_samples < BP_WINDOW_SAMPLES) return false;

    double *ir_filt = (double *)malloc(num_samples * sizeof(double));
    double *red_filt = (double *)malloc(num_samples * sizeof(double));
    double *ecg_filt = (double *)malloc(num_samples * sizeof(double));
    double *v_ir = (double *)malloc(num_samples * sizeof(double));
    double *a_ir = (double *)malloc(num_samples * sizeof(double));
    double *v_red = (double *)malloc(num_samples * sizeof(double));
    double *a_red = (double *)malloc(num_samples * sizeof(double));

    double *ecg_diff = (double *)malloc(num_samples * sizeof(double));
    double *ecg_qrs  = (double *)malloc(num_samples * sizeof(double));

    if (!ir_filt || !red_filt || !ecg_filt || !v_ir || !a_ir || !v_red || !a_red || !ecg_diff || !ecg_qrs) {
        free(ir_filt); free(red_filt); free(ecg_filt);
        free(v_ir); free(a_ir); free(v_red); free(a_red);
        free(ecg_diff); free(ecg_qrs);
        return false;
    }

    // 1. Filter Signals via 100 Hz SOS Engines (Zero-Phase FiltFilt)
    ppg_filtfilt(raw_ppg_ir, ir_filt, (int)num_samples);
    ppg_filtfilt(raw_ppg_red, red_filt, (int)num_samples);
    ecg_filtfilt(raw_ecg, ecg_filt, (int)num_samples);

    bp_min_max_normalize(ir_filt, num_samples);
    bp_min_max_normalize(red_filt, num_samples);
    bp_min_max_normalize(ecg_filt, num_samples);

    // 2. Pan-Tompkins QRS Energy Integration for ECG R-Peaks
    ecg_diff[0] = ecg_filt[1] - ecg_filt[0];
    for (size_t i = 1; i < num_samples - 1; i++) {
        ecg_diff[i] = (ecg_filt[i + 1] - ecg_filt[i - 1]) / 2.0;
    }
    ecg_diff[num_samples - 1] = ecg_filt[num_samples - 1] - ecg_filt[num_samples - 2];

    for (size_t i = 0; i < num_samples; i++) {
        double d2 = ecg_diff[i] * ecg_diff[i];
        double sum = 0.0;
        int count = 0;
        for (int k = -7; k <= 7; k++) {
            int idx = (int)i + k;
            if (idx >= 0 && idx < (int)num_samples) {
                double diff_k = (idx == 0) ? (ecg_filt[1] - ecg_filt[0]) :
                                (idx == (int)num_samples - 1) ? (ecg_filt[idx] - ecg_filt[idx-1]) :
                                (ecg_filt[idx+1] - ecg_filt[idx-1]) / 2.0;
                sum += diff_k * diff_k;
                count++;
            }
        }
        ecg_qrs[i] = (count > 0) ? (sum / (double)count) : d2;
    }
    bp_min_max_normalize(ecg_qrs, num_samples);

    // 3. Peak Detection
    int ecg_peaks[128], ir_peaks[128], red_peaks[128];
    int ecg_cnt = find_peaks_1d(ecg_qrs, num_samples, (int)(0.35 * BP_SAMPLING_RATE_HZ), 0.10, ecg_peaks, 128);
    int ir_cnt  = find_peaks_1d(ir_filt,  num_samples, (int)(0.35 * BP_SAMPLING_RATE_HZ), 0.05, ir_peaks,  128);
    int red_cnt = find_peaks_1d(red_filt, num_samples, (int)(0.35 * BP_SAMPLING_RATE_HZ), 0.05, red_peaks, 128);

    if (ecg_cnt < 2 || ir_cnt < 2 || red_cnt < 2) {
        free(ir_filt); free(red_filt); free(ecg_filt);
        free(v_ir); free(a_ir); free(v_red); free(a_red);
        free(ecg_diff); free(ecg_qrs);
        return false;
    }

    bp_compute_derivatives(ir_filt, v_ir, a_ir, num_samples);
    bp_compute_derivatives(red_filt, v_red, a_red, num_samples);

    double pat_p_ir[128], pat_f_ir[128], pat_d_ir[128];
    double pat_p_red[128], pat_f_red[128], pat_d_red[128];
    double ptt_inter_p[128], ptt_inter_f[128];
    int n_pat_ir = 0, n_pat_red = 0, n_ptt = 0;

    for (int e = 0; e < ecg_cnt; e++) {
        int r_i = ecg_peaks[e];
        int p_ir_idx = -1, p_red_idx = -1;
        int f_ir_idx = -1, f_red_idx = -1;

        for (int p = 0; p < ir_cnt; p++) {
            if (ir_peaks[p] > r_i && ir_peaks[p] < r_i + (int)(0.60 * BP_SAMPLING_RATE_HZ)) {
                p_ir_idx = ir_peaks[p];
                break;
            }
        }
        for (int p = 0; p < red_cnt; p++) {
            if (red_peaks[p] > r_i && red_peaks[p] < r_i + (int)(0.60 * BP_SAMPLING_RATE_HZ)) {
                p_red_idx = red_peaks[p];
                break;
            }
        }

        if (p_ir_idx >= 0) {
            pat_p_ir[n_pat_ir] = (double)(p_ir_idx - r_i) / BP_SAMPLING_RATE_HZ * 1000.0;
            int s_ir = (p_ir_idx - (int)(0.30 * BP_SAMPLING_RATE_HZ) > 0) ? (p_ir_idx - (int)(0.30 * BP_SAMPLING_RATE_HZ)) : 0;
            f_ir_idx = s_ir;
            double min_v = ir_filt[s_ir];
            for (int k = s_ir + 1; k < p_ir_idx; k++) {
                if (ir_filt[k] < min_v) {
                    min_v = ir_filt[k];
                    f_ir_idx = k;
                }
            }
            pat_f_ir[n_pat_ir] = (double)(f_ir_idx - r_i) / BP_SAMPLING_RATE_HZ * 1000.0;

            if (f_ir_idx < p_ir_idx) {
                int d_i = f_ir_idx;
                double max_dv = v_ir[f_ir_idx];
                for (int k = f_ir_idx + 1; k < p_ir_idx; k++) {
                    if (v_ir[k] > max_dv) {
                        max_dv = v_ir[k];
                        d_i = k;
                    }
                }
                pat_d_ir[n_pat_ir] = (double)(d_i - r_i) / BP_SAMPLING_RATE_HZ * 1000.0;
            } else {
                pat_d_ir[n_pat_ir] = (pat_f_ir[n_pat_ir] + pat_p_ir[n_pat_ir]) / 2.0;
            }
            n_pat_ir++;
        }

        if (p_red_idx >= 0) {
            pat_p_red[n_pat_red] = (double)(p_red_idx - r_i) / BP_SAMPLING_RATE_HZ * 1000.0;
            int s_red = (p_red_idx - (int)(0.30 * BP_SAMPLING_RATE_HZ) > 0) ? (p_red_idx - (int)(0.30 * BP_SAMPLING_RATE_HZ)) : 0;
            f_red_idx = s_red;
            double min_vr = red_filt[s_red];
            for (int k = s_red + 1; k < p_red_idx; k++) {
                if (red_filt[k] < min_vr) {
                    min_vr = red_filt[k];
                    f_red_idx = k;
                }
            }
            pat_f_red[n_pat_red] = (double)(f_red_idx - r_i) / BP_SAMPLING_RATE_HZ * 1000.0;

            if (f_red_idx < p_red_idx) {
                int d_i_r = f_red_idx;
                double max_dvr = v_red[f_red_idx];
                for (int k = f_red_idx + 1; k < p_red_idx; k++) {
                    if (v_red[k] > max_dvr) {
                        max_dvr = v_red[k];
                        d_i_r = k;
                    }
                }
                pat_d_red[n_pat_red] = (double)(d_i_r - r_i) / BP_SAMPLING_RATE_HZ * 1000.0;
            } else {
                pat_d_red[n_pat_red] = (pat_f_red[n_pat_red] + pat_p_red[n_pat_red]) / 2.0;
            }
            n_pat_red++;
        }

        if (p_ir_idx >= 0 && p_red_idx >= 0) {
            ptt_inter_p[n_ptt] = (double)(p_red_idx - p_ir_idx) / BP_SAMPLING_RATE_HZ * 1000.0;
            if (f_ir_idx >= 0 && f_red_idx >= 0) {
                ptt_inter_f[n_ptt] = (double)(f_red_idx - f_ir_idx) / BP_SAMPLING_RATE_HZ * 1000.0;
            } else {
                ptt_inter_f[n_ptt] = ptt_inter_p[n_ptt];
            }
            n_ptt++;
        }
    }

    if (n_pat_ir == 0) {
        free(ir_filt); free(red_filt); free(ecg_filt);
        free(v_ir); free(a_ir); free(v_red); free(a_red);
        free(ecg_diff); free(ecg_qrs);
        return false;
    }

    double sum_p = 0.0, sum_f = 0.0, sum_d = 0.0;
    for (int i = 0; i < n_pat_ir; i++) {
        sum_p += pat_p_ir[i];
        sum_f += pat_f_ir[i];
        sum_d += pat_d_ir[i];
    }
    double pat_p_mean = sum_p / n_pat_ir;
    double pat_f_mean = sum_f / n_pat_ir;
    double pat_d_mean = sum_d / n_pat_ir;

    double ptt_inter_peak = 0.0, ptt_inter_foot = 0.0;
    if (n_ptt > 0) {
        for (int i = 0; i < n_ptt; i++) {
            ptt_inter_peak += ptt_inter_p[i];
            ptt_inter_foot += ptt_inter_f[i];
        }
        ptt_inter_peak /= n_ptt;
        ptt_inter_foot /= n_ptt;
    }

    double delta_pat_peak_red_ir = 0.0, delta_pat_foot_red_ir = 0.0, delta_pat_deriv_red_ir = 0.0;
    if (n_pat_red > 0) {
        double sum_pr = 0.0, sum_fr = 0.0, sum_dr = 0.0;
        for (int i = 0; i < n_pat_red; i++) {
            sum_pr += pat_p_red[i];
            sum_fr += pat_f_red[i];
            sum_dr += pat_d_red[i];
        }
        delta_pat_peak_red_ir = (sum_pr / n_pat_red) - pat_p_mean;
        delta_pat_foot_red_ir = (sum_fr / n_pat_red) - pat_f_mean;
        delta_pat_deriv_red_ir = (sum_dr / n_pat_red) - pat_d_mean;
    }

    double rr_sec = 0.8;
    if (ecg_cnt >= 2) {
        double total_rr = 0.0;
        for (int i = 1; i < ecg_cnt; i++) {
            total_rr += (double)(ecg_peaks[i] - ecg_peaks[i-1]);
        }
        rr_sec = (total_rr / (double)(ecg_cnt - 1)) / BP_SAMPLING_RATE_HZ;
    }
    if (rr_sec < 0.3) rr_sec = 0.3;
    if (rr_sec > 2.0) rr_sec = 2.0;

    double pep_est = 60.0 + 0.12 * (1000.0 * rr_sec) * 0.1;
    double ptt_p_est = pat_p_mean - pep_est;
    double ptt_f_est = pat_f_mean - pep_est;
    double ptt_d_est = pat_d_mean - pep_est;

    double pat_f_bazett = pat_f_mean / sqrt(rr_sec + 1e-5);
    double pat_d_bazett = pat_d_mean / sqrt(rr_sec + 1e-5);
    double pat_p_bazett = pat_p_mean / sqrt(rr_sec + 1e-5);

    double pat_f_fridericia = pat_f_mean / (cbrt(rr_sec) + 1e-5);
    double pat_d_fridericia = pat_d_mean / (cbrt(rr_sec) + 1e-5);
    double pat_p_fridericia = pat_p_mean / (cbrt(rr_sec) + 1e-5);

    double pat_f_framingham = pat_f_mean + 0.154 * (1.0 - rr_sec) * 1000.0;
    double pat_d_framingham = pat_d_mean + 0.154 * (1.0 - rr_sec) * 1000.0;
    double pat_p_framingham = pat_p_mean + 0.154 * (1.0 - rr_sec) * 1000.0;

    double pat_f_inv = 1.0 / (pat_f_mean + 1e-5);
    double pat_f_sq_inv = 1.0 / (pat_f_mean * pat_f_mean + 1e-5);
    double pat_d_inv = 1.0 / (pat_d_mean + 1e-5);
    double pat_d_sq_inv = 1.0 / (pat_d_mean * pat_d_mean + 1e-5);
    double pat_p_inv = 1.0 / (pat_p_mean + 1e-5);
    double pat_p_sq_inv = 1.0 / (pat_p_mean * pat_p_mean + 1e-5);

    double ptt_f_inv = 1.0 / (ptt_f_est + 1e-5);
    double ptt_d_inv = 1.0 / (ptt_d_est + 1e-5);
    double ptt_p_inv = 1.0 / (ptt_p_est + 1e-5);
    double ptt_f_sq_inv = 1.0 / (ptt_f_est * ptt_f_est + 1e-5);
    double ptt_d_sq_inv = 1.0 / (ptt_d_est * ptt_d_est + 1e-5);
    double ptt_p_sq_inv = 1.0 / (ptt_p_est * ptt_p_est + 1e-5);

    double t_sys_dia = pat_p_mean - pat_f_mean;
    double t_sys_deriv = pat_d_mean - pat_f_mean;
    double t_deriv_dia = pat_p_mean - pat_d_mean;

    // AC/DC dynamics on raw signals
    double min_raw_ir = raw_ppg_ir[0], max_raw_ir = raw_ppg_ir[0], sum_raw_ir = 0.0;
    double min_raw_red = raw_ppg_red[0], max_raw_red = raw_ppg_red[0], sum_raw_red = 0.0;
    double sum_sq_vir = 0.0, sum_sq_air = 0.0, sum_sq_vred = 0.0, sum_sq_ared = 0.0;

    for (size_t i = 0; i < num_samples; i++) {
        if (raw_ppg_ir[i] < min_raw_ir) min_raw_ir = raw_ppg_ir[i];
        if (raw_ppg_ir[i] > max_raw_ir) max_raw_ir = raw_ppg_ir[i];
        sum_raw_ir += raw_ppg_ir[i];

        if (raw_ppg_red[i] < min_raw_red) min_raw_red = raw_ppg_red[i];
        if (raw_ppg_red[i] > max_raw_red) max_raw_red = raw_ppg_red[i];
        sum_raw_red += raw_ppg_red[i];

        sum_sq_vir += v_ir[i] * v_ir[i];
        sum_sq_air += a_ir[i] * a_ir[i];
        sum_sq_vred += v_red[i] * v_red[i];
        sum_sq_ared += a_red[i] * a_red[i];
    }

    double ac_ir = max_raw_ir - min_raw_ir;
    double dc_ir = (sum_raw_ir / (double)num_samples) + 1e-5;
    double pi_ir = (ac_ir / fabs(dc_ir)) * 100.0;

    double ac_red = max_raw_red - min_raw_red;
    double dc_red = (sum_raw_red / (double)num_samples) + 1e-5;
    double pi_red = (ac_red / fabs(dc_red)) * 100.0;

    double r_optical_ratio = (ac_red / dc_red) / (ac_ir / dc_ir + 1e-5);
    double ac_dc_ratio = (ac_red + ac_ir) / (dc_red + dc_ir + 1e-5);

    double ir_vpg_rms = sqrt(sum_sq_vir / (double)num_samples);
    double ir_apg_rms = sqrt(sum_sq_air / (double)num_samples);
    double red_vpg_rms = sqrt(sum_sq_vred / (double)num_samples);
    double red_apg_rms = sqrt(sum_sq_ared / (double)num_samples);
    double ir_shr = ir_vpg_rms / (ir_apg_rms + 1e-5);
    double red_shr = red_vpg_rms / (red_apg_rms + 1e-5);

    morph_features_t ir_m = compute_morphology(ir_filt, num_samples);
    morph_features_t red_m = compute_morphology(red_filt, num_samples);
    double k_val = ir_m.tsys / (t_sys_dia + 1e-5);

    features_out[FEAT_PAT_F]                  = pat_f_mean;
    features_out[FEAT_PAT_D]                  = pat_d_mean;
    features_out[FEAT_PAT_P]                  = pat_p_mean;
    features_out[FEAT_PAT_F_BAZETT]           = pat_f_bazett;
    features_out[FEAT_PAT_D_BAZETT]           = pat_d_bazett;
    features_out[FEAT_PAT_P_BAZETT]           = pat_p_bazett;
    features_out[FEAT_PAT_F_FRIDERICIA]       = pat_f_fridericia;
    features_out[FEAT_PAT_D_FRIDERICIA]       = pat_d_fridericia;
    features_out[FEAT_PAT_P_FRIDERICIA]       = pat_p_fridericia;
    features_out[FEAT_PAT_F_FRAMINGHAM]       = pat_f_framingham;
    features_out[FEAT_PAT_D_FRAMINGHAM]       = pat_d_framingham;
    features_out[FEAT_PAT_P_FRAMINGHAM]       = pat_p_framingham;
    features_out[FEAT_PAT_F_INV]              = pat_f_inv;
    features_out[FEAT_PAT_F_SQ_INV]           = pat_f_sq_inv;
    features_out[FEAT_PAT_D_INV]              = pat_d_inv;
    features_out[FEAT_PAT_D_SQ_INV]           = pat_d_sq_inv;
    features_out[FEAT_PAT_P_INV]              = pat_p_inv;
    features_out[FEAT_PAT_P_SQ_INV]           = pat_p_sq_inv;
    features_out[FEAT_PTT_P_EST]              = ptt_p_est;
    features_out[FEAT_PTT_F_EST]              = ptt_f_est;
    features_out[FEAT_PTT_D_EST]              = ptt_d_est;
    features_out[FEAT_PTT_F_INV]              = ptt_f_inv;
    features_out[FEAT_PTT_D_INV]              = ptt_d_inv;
    features_out[FEAT_PTT_P_INV]              = ptt_p_inv;
    features_out[FEAT_PTT_F_SQ_INV]           = ptt_f_sq_inv;
    features_out[FEAT_PTT_D_SQ_INV]           = ptt_d_sq_inv;
    features_out[FEAT_PTT_P_SQ_INV]           = ptt_p_sq_inv;
    features_out[FEAT_PTT_INTER_PEAK]         = ptt_inter_peak;
    features_out[FEAT_PTT_INTER_FOOT]         = ptt_inter_foot;
    features_out[FEAT_DELTA_PAT_PEAK_RED_IR]  = delta_pat_peak_red_ir;
    features_out[FEAT_DELTA_PAT_FOOT_RED_IR]  = delta_pat_foot_red_ir;
    features_out[FEAT_DELTA_PAT_DERIV_RED_IR] = delta_pat_deriv_red_ir;
    features_out[FEAT_T_SYS_DIA]              = t_sys_dia;
    features_out[FEAT_T_SYS_DERIV]            = t_sys_deriv;
    features_out[FEAT_T_DERIV_DIA]            = t_deriv_dia;
    features_out[FEAT_PW25]                   = ir_m.pw25;
    features_out[FEAT_PW50]                   = ir_m.pw50;
    features_out[FEAT_PW75]                   = ir_m.pw75;
    features_out[FEAT_K_VAL]                  = k_val;
    features_out[FEAT_AREA_RATIO]             = ir_m.area_ratio;
    features_out[FEAT_AIX]                    = ir_m.aix;
    features_out[FEAT_AIX_RED]                = red_m.aix;
    features_out[FEAT_PI_IR]                  = pi_ir;
    features_out[FEAT_PI_RED]                 = pi_red;
    features_out[FEAT_R_OPTICAL_RATIO]        = r_optical_ratio;
    features_out[FEAT_AC_DC_RATIO]            = ac_dc_ratio;
    features_out[FEAT_IR_VPG_RMS]             = ir_vpg_rms;
    features_out[FEAT_IR_APG_RMS]             = ir_apg_rms;
    features_out[FEAT_RED_VPG_RMS]            = red_vpg_rms;
    features_out[FEAT_RED_APG_RMS]            = red_apg_rms;
    features_out[FEAT_IR_SHR]                 = ir_shr;
    features_out[FEAT_RED_SHR]                = red_shr;
    features_out[FEAT_IR_TSYS]                = ir_m.tsys;
    features_out[FEAT_IR_DECAY_SLOPE]         = ir_m.decay_slope;
    features_out[FEAT_IR_AREA_A1]             = ir_m.area_a1;
    features_out[FEAT_IR_AREA_A2]             = ir_m.area_a2;
    features_out[FEAT_IR_IPA_RATIO]           = ir_m.ipa_ratio;
    features_out[FEAT_IR_APG_B_A]             = ir_m.apg_b_a;
    features_out[FEAT_IR_APG_AGI]             = ir_m.apg_agi;
    features_out[FEAT_RED_TSYS]               = red_m.tsys;
    features_out[FEAT_RED_DECAY_SLOPE]        = red_m.decay_slope;
    features_out[FEAT_RED_PW25]               = red_m.pw25;
    features_out[FEAT_RED_PW50]               = red_m.pw50;
    features_out[FEAT_RED_PW75]               = red_m.pw75;
    features_out[FEAT_RED_AREA_A1]            = red_m.area_a1;
    features_out[FEAT_RED_AREA_A2]            = red_m.area_a2;
    features_out[FEAT_RED_IPA_RATIO]          = red_m.ipa_ratio;
    features_out[FEAT_RED_APG_B_A]            = red_m.apg_b_a;
    features_out[FEAT_RED_APG_AGI]            = red_m.apg_agi;
    features_out[FEAT_SEX]                    = is_male;

    free(ir_filt); free(red_filt); free(ecg_filt);
    free(v_ir); free(a_ir); free(v_red); free(a_red);
    free(ecg_diff); free(ecg_qrs);
    return true;
}

bool bp_predict_from_raw(
    const double *raw_ppg_red,
    const double *raw_ppg_ir,
    const double *raw_ecg,
    size_t num_samples,
    double is_male,
    bp_prediction_result_t *result
) {
    if (!result) return false;

    double features[NUM_INPUT_FEATURES];
    bool ok = bp_extract_features(raw_ppg_red, raw_ppg_ir, raw_ecg, num_samples, is_male, features);
    if (!ok) return false;

    result->sbp = predict_sbp(features);
    result->dbp = predict_dbp(features);
    result->map_calc = result->dbp + (result->sbp - result->dbp) / 3.0;

    double rr_sec = (features[FEAT_PAT_F] / (features[FEAT_PAT_F_BAZETT] + 1e-5));
    rr_sec = rr_sec * rr_sec;
    if (rr_sec > 0.3 && rr_sec < 2.0) {
        result->heart_rate_bpm = 60.0 / rr_sec;
    } else {
        result->heart_rate_bpm = 75.0;
    }

    result->pat_foot_ms       = features[FEAT_PAT_F];
    result->pat_peak_ms       = features[FEAT_PAT_P];
    result->ptt_inter_peak_ms = features[FEAT_PTT_INTER_PEAK];
    result->pulse_width_50_ms = features[FEAT_PW50];
    result->stiffness_k_val   = features[FEAT_K_VAL];
    result->optical_ratio_r   = features[FEAT_R_OPTICAL_RATIO];
    result->num_detected_beats = (int)(num_samples / (BP_SAMPLING_RATE_HZ * rr_sec + 1e-5));

    return true;
}
