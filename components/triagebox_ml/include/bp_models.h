#ifndef BP_MODELS_H
#define BP_MODELS_H

#include "lgbm_sbp.h"
#include "lgbm_dbp.h"

#define NUM_INPUT_FEATURES 23

#ifdef __cplusplus
extern "C" {
#endif

static inline double predict_sbp(double *features) {
    return score_sbp(features);
}

static inline double predict_dbp(double *features) {
    return score_dbp(features);
}

#define FEAT_PAT_D                     0
#define FEAT_PAT_P                     1
#define FEAT_PAT_F_FRIDERICIA          2
#define FEAT_PAT_F_FRAMINGHAM          3
#define FEAT_PAT_D_FRAMINGHAM          4
#define FEAT_PAT_D_INV                 5
#define FEAT_PAT_D_SQ_INV              6
#define FEAT_PAT_P_SQ_INV              7
#define FEAT_PTT_P_EST                 8
#define FEAT_PTT_F_EST                 9
#define FEAT_PTT_F_SQ_INV              10
#define FEAT_PTT_D_SQ_INV              11
#define FEAT_PTT_P_SQ_INV              12
#define FEAT_PW75                      13
#define FEAT_K_VAL                     14
#define FEAT_PI_IR                     15
#define FEAT_AC_DC_RATIO               16
#define FEAT_IR_VPG_RMS                17
#define FEAT_IR_APG_RMS                18
#define FEAT_RED_VPG_RMS               19
#define FEAT_IR_TSYS                   20
#define FEAT_RED_PW50                  21
#define FEAT_SEX                       22

#ifdef __cplusplus
}
#endif

#endif // BP_MODELS_H
