# Triage inference. The model files (triage_pipeline.c, bp_*) are the ML side's;
# tb_triage.c + tb_triage_model.c are the adapter the UI calls -- see tb_triage.h
# for why they exist and why they are two files. Selftests are host-only, run by
# tools/run_selftests.sh.
#
# triage_demo.c and predict_bp_example.c are examples, deliberately not built.
# triage_demo.c does not compile as it stands (an unfinished assignment to
# input.airway_problem); harmless while it stays out of this list.
idf_component_register(
    SRCS
       tb_triage.c
       tb_triage_model.c
       triage_pipeline.c
       bp_pipeline.c
       lgbm_sbp.c
       lgbm_dbp.c
    INCLUDE_DIRS
        "include"
        # ui_types.h: vitals_t and ui_priority_t are the model's in/out contract.
        "../../ui/logic"
)

# score_lgbm_multiclass() is one ~67k-line function, and that is what breaks the
# build on Xtensa:
#
#   dangerous relocation: l32r: literal target out of range
#
# l32r reaches backwards a limited distance, and by default every literal for a
# function goes in one pool at the start of its .literal section. In a function
# this size the far end of the code is simply too far from that pool.
#
# -mtext-section-literals alone is NOT enough here -- it only moves the failure
# from the linker into the assembler ("operand 2 of 'l32r' has out of range
# value"), because there is still one pool per section. -mauto-litpools lets the
# assembler drop extra pools mid-function wherever the reach runs out, which is
# the only option that scales to a function this large.
#
# Scoped to this one file rather than the component, let alone the project: it
# changes code layout, and nothing else here is remotely big enough to need it.
# It goes away by itself if the model is ever emitted as a data table with a
# small interpreter instead of unrolled control flow -- which would also cut the
# 72k lines and the compile time that goes with them.
set_source_files_properties(triage_pipeline.c PROPERTIES
    COMPILE_OPTIONS "-mauto-litpools")
