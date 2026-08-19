#ifndef BP_MODELS_H
#define BP_MODELS_H

#include "lgbm_sbp.h"
#include "lgbm_dbp.h"

#define NUM_INPUT_FEATURES 72

#define FEAT_PPG_MEAN             0
#define FEAT_PPG_VAR              1
#define FEAT_PPG_ABS_ENERGY       2
#define FEAT_PAT_F                3
#define FEAT_PAT_D                4
#define FEAT_PAT_P                5
#define FEAT_PAT_F_BAZETT         6
#define FEAT_PAT_D_BAZETT         7
#define FEAT_PAT_P_BAZETT         8
#define FEAT_PAT_F_FRIDERICIA     9
#define FEAT_PAT_D_FRIDERICIA     10
#define FEAT_PAT_P_FRIDERICIA     11
#define FEAT_LOG_PAT_F            12
#define FEAT_LOG_PAT_D            13
#define FEAT_LOG_PAT_P            14
#define FEAT_INV_PAT_F            15
#define FEAT_INV_PAT_F2           16
#define FEAT_INV_PAT_D            17
#define FEAT_INV_PAT_D2           18
#define FEAT_INV_PAT_P            19
#define FEAT_INV_PAT_P2           20
#define FEAT_DELTA_PAT_PF         21
#define FEAT_DELTA_PAT_DF         22
#define FEAT_DELTA_PAT_PD         23
#define FEAT_RATIO_PF             24
#define FEAT_RATIO_DF             25
#define FEAT_RATIO_PD             26
#define FEAT_SLOPE_PF             27
#define FEAT_SLOPE_DF             28
#define FEAT_SLOPE_PD             29
#define FEAT_PTT_INTER_PEAK       30
#define FEAT_PTT_INTER_FOOT       31
#define FEAT_PTT_ESTIMATED        32
#define FEAT_V_MAX                33
#define FEAT_V_MIN                34
#define FEAT_A_MAX                35
#define FEAT_A_MIN                36
#define FEAT_PW25                 37
#define FEAT_PW50                 38
#define FEAT_PW75                 39
#define FEAT_K_VAL                40
#define FEAT_AREA_RATIO           41
#define FEAT_AIX                  42
#define FEAT_AIX_RED              43
#define FEAT_PI_IR                44
#define FEAT_PI_RED               45
#define FEAT_R_OPTICAL_RATIO      46
#define FEAT_AC_DC_RATIO          47
#define FEAT_IR_VPG_RMS           48
#define FEAT_IR_APG_RMS           49
#define FEAT_RED_VPG_RMS          50
#define FEAT_RED_APG_RMS          51
#define FEAT_IR_SHR               52
#define FEAT_RED_SHR              53
#define FEAT_IR_TSYS              54
#define FEAT_IR_DECAY_SLOPE       55
#define FEAT_IR_AREA_A1           56
#define FEAT_IR_AREA_A2           57
#define FEAT_IR_IPA_RATIO         58
#define FEAT_IR_APG_B_A           59
#define FEAT_IR_APG_AGI           60
#define FEAT_RED_TSYS             61
#define FEAT_RED_DECAY_SLOPE      62
#define FEAT_RED_PW25             63
#define FEAT_RED_PW50             64
#define FEAT_RED_PW75             65
#define FEAT_RED_AREA_A1          66
#define FEAT_RED_AREA_A2          67
#define FEAT_RED_IPA_RATIO        68
#define FEAT_RED_APG_B_A          69
#define FEAT_RED_APG_AGI          70
#define FEAT_SEX                  71

#endif // BP_MODELS_H
