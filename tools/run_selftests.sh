#!/bin/sh
# Compile and run every *_selftest.c in the tree on the host. No framework:
# each file is a main() full of asserts. Fails loudly on the first bad one.
set -eu
cd "$(dirname "$0")/.."

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
CC=${CC:-cc}
# ASan is skipped under MSYS2/Git Bash: its interceptors cannot hook memcpy on
# Windows, so every binary dies with "CHECK failed: ... (real_memcpy) != (0)"
# before reaching main. The asserts are the actual test and run either way.
# Override with SAN= to force off, or SAN=-fsanitize=undefined for UBSan alone.
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) : "${SAN=}" ;;
*)                        : "${SAN=-fsanitize=address,undefined}" ;;
esac
# UI_MEASURE_MS: the hardware default is 60 s (RR needs a minute of microphone),
# which would make the runtime selftest wait a simulated minute for no benefit.
# 2 s here matches what sim/CMakeLists.txt passes, and the two timing tests state
# the number they assume.
FLAGS="-std=c99 -Wall -Wextra -Werror -g -DUI_MEASURE_MS=2000 $SAN"
[ -n "$SAN" ] || echo "note: sanitizers off, assertions still checked"
fail=0

run() { # run <selftest.c> <extra .c files...>
    name=$(basename "$1" .c)
    shift
    # -I every dir holding a header we might need; harmless if unused.
    if ! $CC $FLAGS -I ui/logic -I components/triagebox_link/include \
        -I components/triagebox_ml/include \
        -I components/triagebox_board/test_fakes \
        -o "$OUT/$name" "$@" -lm 2>"$OUT/$name.log"; then
        echo "FAIL (compile) $name"; cat "$OUT/$name.log"; fail=1; return
    fi
    if ! "$OUT/$name"; then
        echo "FAIL (run) $name"; fail=1
    fi
}

run components/triagebox_link/tb_frame_selftest.c \
    components/triagebox_link/tb_frame_selftest.c \
    components/triagebox_link/tb_frame.c \
    ui/logic/ui_types.c

# Snapshot decode + button state-to-edge diff. tb_i2c_codec.c is deliberately
# ESP-IDF-free so the real file runs here, not a copy.
run components/triagebox_link/tb_i2c_codec_selftest.c \
    components/triagebox_link/tb_i2c_codec_selftest.c \
    components/triagebox_link/tb_i2c_codec.c \
    ui/logic/ui_types.c

# tb_triage.c plus the ML side's ESI->colour mapping. tb_triage_model.c is NOT
# linked -- it pulls in the 72k-line GBM. Instead the selftest includes
# tb_classify.h and supplies its own predict_triage(), so the mapping is checked
# without the model.
run components/triagebox_ml/tb_triage_selftest.c \
    components/triagebox_ml/tb_triage_selftest.c \
    components/triagebox_ml/tb_triage.c \
    ui/logic/ui_types.c

# Pre-existing UI logic selftests. ui_bindings/ui_input need LVGL, so the ones
# listed here are the LVGL-free modules only.
run ui/logic/ui_mock_selftest.c \
    ui/logic/ui_mock_selftest.c ui/logic/ui_mock.c ui/logic/ui_types.c

run ui/logic/ui_nav_selftest.c \
    ui/logic/ui_nav_selftest.c ui/logic/ui_nav.c ui/logic/ui_session.c \
    ui/logic/ui_action.c ui/logic/ui_types.c

run ui/logic/ui_runtime_selftest.c \
    ui/logic/ui_runtime_selftest.c ui/logic/ui_runtime.c ui/logic/ui_mock.c \
    ui/logic/ui_nav.c ui/logic/ui_session.c ui/logic/ui_action.c ui/logic/ui_types.c

run ui/logic/ui_status_selftest.c \
    ui/logic/ui_status_selftest.c ui/logic/ui_status.c

# Demo mode: the fake vitals have to agree with the RED verdict it hardcodes.
run ui/logic/ui_demo_selftest.c \
    ui/logic/ui_demo_selftest.c ui/logic/ui_demo.c

# The real ui_board.c, compiled against components/triagebox_board/test_fakes/
# instead of ESP-IDF. It is the only function in the tree that cuts power to a
# running device, so the sequence is worth pinning on the host.
run components/triagebox_board/ui_board_power_selftest.c \
    components/triagebox_board/ui_board_power_selftest.c \
    components/triagebox_board/ui_board.c

# The real tb_ui_source.c against test_fakes/ as well: the RFID gate decides
# which patient's ID a set of vitals is filed under.
run components/triagebox_link/tb_ui_source_selftest.c \
    components/triagebox_link/tb_ui_source_selftest.c \
    components/triagebox_link/tb_ui_source.c \
    ui/logic/ui_demo.c \
    ui/logic/ui_types.c

[ "$fail" -eq 0 ] && echo "all selftests OK"
exit "$fail"
