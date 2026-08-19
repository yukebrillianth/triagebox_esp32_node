# Triage inference. Platform-neutral C89-ish math: no ESP-IDF calls, so
# tb_svm_selftest.c can run it on the host (tools/run_selftests.sh) and the
# selftest is not listed here.
idf_component_register(
    SRCS
       triage_pipeline.c
       lgbm_dbp.c
       lgbm_sbp.c
       ppg_filter.c
       ecg_filter.c
    INCLUDE_DIRS
        "include"
        # ui_types.h: vitals_t and ui_priority_t are the model's in/out contract.
        "../../ui/logic"
)
