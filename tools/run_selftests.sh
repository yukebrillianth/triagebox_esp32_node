#!/bin/sh
# Compile and run every *_selftest.c in the tree on the host. No framework:
# each file is a main() full of asserts. Fails loudly on the first bad one.
set -eu
cd "$(dirname "$0")/.."

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
CC=${CC:-cc}
FLAGS="-std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined"
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

run components/triagebox_ml/tb_svm_selftest.c \
    components/triagebox_ml/tb_svm_selftest.c \
    components/triagebox_ml/tb_svm.c

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

# The real ui_board.c, compiled against components/triagebox_board/test_fakes/
# instead of ESP-IDF. It is the only function in the tree that cuts power to a
# running device, so the sequence is worth pinning on the host.
run components/triagebox_board/ui_board_power_selftest.c \
    components/triagebox_board/ui_board_power_selftest.c \
    components/triagebox_board/ui_board.c

[ "$fail" -eq 0 ] && echo "all selftests OK"
exit "$fail"
