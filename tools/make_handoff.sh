#!/bin/sh
# Assemble handoff/triagebox-ui/ — the UI drop for the ESP32 + STM32 developers.
# Re-run after every LVGL Pro export (Ctrl+B): the package is a copy, not a link.
set -eu
cd "$(dirname "$0")/.."
OUT=handoff/triagebox-ui

rm -rf "$OUT"
mkdir -p "$OUT/glue"

# Generated screens/components/images/fonts + hand-written logic.
# XML/preview/ttf sources stay in the UI repo: integrators consume C only.
rsync -a \
  --exclude 'preview-build/' --exclude 'preview-bin/' --exclude 'sim/' \
  --exclude 'assets/' --exclude 'widgets/' \
  --exclude '*.xml' --exclude '*.ttf' --exclude '*.md' --exclude '*_selftest.c' \
  ui/ "$OUT/ui/"

# LVGL Pro also exports template icons/fonts nobody references (1.2 MB of C
# arrays the manifest never compiles). Drop anything not in the manifest.
python3 - "$OUT" <<'PY'
import os, re, sys
out = sys.argv[1] + '/ui'
keep = set(re.findall(r'\$\{CMAKE_CURRENT_LIST_DIR\}/([^\s\)]+)', open(out + '/file_list_gen.cmake').read()))
for sub in ('images', 'fonts'):
    for f in os.listdir(os.path.join(out, sub)):
        p = sub + '/' + f
        if f.endswith(('.c', '.h')) and p not in keep and p[:-2] + '.c' not in keep:
            os.remove(os.path.join(out, p))
PY

rsync -a spiffs_assets/ "$OUT/spiffs_assets/"

cp main/asset_fs.c main/asset_fs.h "$OUT/glue/"
cp main/app_main.c "$OUT/glue/app_main.example.c"
cp main/CMakeLists.txt "$OUT/glue/CMakeLists.example.txt"
cp partitions.csv "$OUT/"
cp sdkconfig.defaults "$OUT/sdkconfig.reference.defaults"
cp docs/integration-esp32-stm32.md "$OUT/INTEGRATION.md"

# Vendored BSP: upstream 1.1.0 does not compile on ESP-IDF v6 (see INTEGRATION.md).
[ -d components/esp32_s3_touch_lcd_4 ] &&
  rsync -a components/esp32_s3_touch_lcd_4/ "$OUT/board/esp32_s3_touch_lcd_4/"

# Every .c the manifest lists must be in the package, or their build breaks late.
missing=$(sed -n 's|.*${CMAKE_CURRENT_LIST_DIR}/||p' "$OUT/ui/file_list_gen.cmake" |
  while read -r f; do [ -f "$OUT/ui/$f" ] || echo "$f"; done)
[ -z "$missing" ] || { echo "MISSING from package:"; echo "$missing"; exit 1; }

echo "OK $(du -sh "$OUT" | cut -f1) — $(find "$OUT" -name '*.c' | wc -l | tr -d ' ') .c files"

# Ship as one zip so nobody receives a half-copied tree. -x drops macOS turds.
(cd handoff && rm -f triagebox-ui.zip &&
  zip -qr triagebox-ui.zip triagebox-ui -x '*.DS_Store' -x '__MACOSX/*')
echo "ZIP handoff/triagebox-ui.zip ($(du -h handoff/triagebox-ui.zip | cut -f1))"
