# Triage inference. The model files (triage_pipeline.c, bp_*, *_filter.c) are the
# ML side's; tb_triage.c is the adapter the UI calls -- see tb_triage.h for why
# it exists. Selftests are host-only, run by tools/run_selftests.sh.
#
# triage_demo.c and predict_bp_example.c are examples, deliberately not built.
idf_component_register(
    SRCS
       tb_triage.c
       tb_triage_model.c
       triage_pipeline.c
       bp_pipeline.c
       lgbm_sbp.c
    INCLUDE_DIRS
        "include"
        # ui_types.h: vitals_t and ui_priority_t are the model's in/out contract.
        "../../ui/logic"
)
