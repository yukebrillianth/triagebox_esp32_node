#!/usr/bin/env bash
# Flash cost of the ML model objects, from an existing ESP-IDF build.
#
#   idf.py build && tools/model_sizes.sh
#
# Runs the toolchain's `size` on each model object (text = code + rodata). For the
# full per-file table use `idf.py size-files`. Needs the ESP-IDF environment
# exported (`. $IDF_PATH/export.sh`) so the xtensa toolchain is on PATH.
set -euo pipefail
SIZE=${SIZE:-xtensa-esp32s3-elf-size}
command -v "$SIZE" >/dev/null || { echo "$SIZE not on PATH: export the ESP-IDF environment first"; exit 1; }
printf '%-22s %10s %10s %10s\n' object text data bss
for name in triage_pipeline lgbm_sbp lgbm_dbp bp_pipeline; do
  obj=$(find build -name "$name.c.obj" | head -1)
  [ -n "$obj" ] || { echo "$name: no object under build/ (run idf.py build)"; continue; }
  "$SIZE" "$obj" | awk -v n="$name" 'NR == 2 { printf "%-22s %10d %10d %10d\n", n, $1, $2, $3 }'
done
