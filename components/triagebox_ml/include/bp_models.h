#ifndef BP_MODELS_H
#define BP_MODELS_H

#include "lgbm_sbp.h"
#include "lgbm_dbp.h"

#define NUM_INPUT_FEATURES 70

#define FEAT_PAT_F 0
#define FEAT_PAT_D 1
#define FEAT_PAT_P 2
#define FEAT_PAT_F_BAZETT 3
#define FEAT_PAT_D_BAZETT 4
#define FEAT_PAT_P_BAZETT 5
#define FEAT_PAT_F_FRIDERICIA 6
#define FEAT_PAT_D_FRIDERICIA 7
#define FEAT_PAT_P_FRIDERICIA 8
#define FEAT_PAT_F_FRAMINGHAM 9
#define FEAT_PAT_D_FRAMINGHAM 10
#define FEAT_PAT_P_FRAMINGHAM 11
#define FEAT_PAT_F_INV 12
#define FEAT_PAT_F_SQ_INV 13
#define FEAT_PAT_D_INV 14
#define FEAT_PAT_D_SQ_INV 15
#define FEAT_PAT_P_INV 16
#define FEAT_PAT_P_SQ_INV 17
#define FEAT_PTT_P_EST 18
#define FEAT_PTT_F_EST 19
#define FEAT_PTT_D_EST 20
#define FEAT_PTT_F_INV 21
#define FEAT_PTT_D_INV 22
#define FEAT_PTT_P_INV 23
#define FEAT_PTT_F_SQ_INV 24
#define FEAT_PTT_D_SQ_INV 25
#define FEAT_PTT_P_SQ_INV 26
#define FEAT_PTT_INTER_PEAK 27
#define FEAT_PTT_INTER_FOOT 28
#define FEAT_DELTA_PAT_PEAK_RED_IR 29
#define FEAT_DELTA_PAT_FOOT_RED_IR 30
#define FEAT_DELTA_PAT_DERIV_RED_IR 31
#define FEAT_T_SYS_DIA 32
#define FEAT_T_SYS_DERIV 33
#define FEAT_T_DERIV_DIA 34
#define FEAT_PW25 35
#define FEAT_PW50 36
#define FEAT_PW75 37
#define FEAT_K_VAL 38
#define FEAT_AREA_RATIO 39
#define FEAT_AIX 40
#define FEAT_AIX_RED 41
#define FEAT_PI_IR 42
#define FEAT_PI_RED 43
#define FEAT_R_OPTICAL_RATIO 44
#define FEAT_AC_DC_RATIO 45
#define FEAT_IR_VPG_RMS 46
#define FEAT_IR_APG_RMS 47
#define FEAT_RED_VPG_RMS 48
#define FEAT_RED_APG_RMS 49
#define FEAT_IR_SHR 50
#define FEAT_RED_SHR 51
#define FEAT_IR_TSYS 52
#define FEAT_IR_DECAY_SLOPE 53
#define FEAT_IR_AREA_A1 54
#define FEAT_IR_AREA_A2 55
#define FEAT_IR_IPA_RATIO 56
#define FEAT_IR_APG_B_A 57
#define FEAT_IR_APG_AGI 58
#define FEAT_RED_TSYS 59
#define FEAT_RED_DECAY_SLOPE 60
#define FEAT_RED_PW25 61
#define FEAT_RED_PW50 62
#define FEAT_RED_PW75 63
#define FEAT_RED_AREA_A1 64
#define FEAT_RED_AREA_A2 65
#define FEAT_RED_IPA_RATIO 66
#define FEAT_RED_APG_B_A 67
#define FEAT_RED_APG_AGI 68
#define FEAT_SEX 69

static inline double predict_sbp(double *features) {
    return score_sbp(features);
}

static inline double predict_dbp(double *features) {
    return score_dbp(features);
}

#endif /* BP_MODELS_H */
