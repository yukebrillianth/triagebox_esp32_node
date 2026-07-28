
## Task 3 — UI data contracts
- Mirrored canonical backend vital names exactly: `hr`, `spo2`, `rr`, `bp_sys`, `bp_dia`, `battery`.
- Backend accepts aliases, but the shared UI contract deliberately uses only canonical snake_case names to avoid serial/LoRa renaming.
- `vitals_t.valid` means the whole reading set is usable and not stale; RFID absence is represented independently by `rfid_t.present == false`.

## Task 13 — ui_mock.c/.h
- Non-blocking design: all state (scan/measure) keyed off an internal `s_now_ms` set by `ui_mock_tick(now_ms)`; no timers, threads, or delay calls.
- RFID `ui_mock_rfid_ready()` is one-shot: consumes the ready flag on read so callers don't get the same tag twice per scan.
- Priority cycle order is GREEN→YELLOW→RED→BLACK (not enum declaration order RED,YELLOW,GREEN,BLACK) — cycle table `k_cycle[]` decouples QA order from `ui_types.h` enum order.
- Vitals jitter uses a tiny xorshift32 PRNG (no libc rand) for determinism across host/sim/hardware builds.
- `UI_MEASURE_MS` guarded by `#ifndef` so a later hardware build can override to 60000 via build flag without editing this file.
- Left `ui/logic/ui_mock_selftest.c` as the one runnable self-check (assert-based, no framework) per lazy-dev convention — compile+run it standalone against ui_mock.c whenever this file changes.

- Task 12: ui_session uses one static state, explicit has_age/has_gender/has_priority flags, bounded reasons storage, and clamps progress to 100. Reset zeroes all data and leaves enum getters at documented inert defaults.

## Task 7 — ESP-IDF target skeleton
- Build passed with `source ~/.espressif/v6.0.2/esp-idf/export.sh > /dev/null 2>&1 && idf.py set-target esp32s3 && idf.py build`; source ESP-IDF export.sh in every shell before `idf.py`.
- Resolved LVGL version: 9.5.0.

## Task 14 — mock-buffer keypad indev
- LVGL v9 keypad `read_cb` consumes only `ui_mock_pop_button()`: buttons 0/1/2/3 map to PREV/NEXT/ENTER/ESC, and release/no-event reports preserve the last key.
- `ui_input_create_group()` deliberately only creates a group; screen object membership and `lv_indev_set_group()` remain Task 24 concerns.
- Planned simulator producer remains Task 26: SDL keys 1/2/3/4 push mock buttons 0/1/2/3.
