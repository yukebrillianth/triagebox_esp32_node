
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

## [2026-07-28T16:45Z] Fonts + icons from Pro template (T4/T5 inventory)
- Pro ships Montserrat (Regular/Medium/SemiBold/Bold) + FontAwesome5, NOT Inter.
  Acceptable metric substitute per plan Key Decision "Inter or metric substitute".
- Compiled font C already present under ui/fonts/ as font_body/h1-h5 *_data.c with Latin 0x20-0x7F.
  Figma sizes 48/24/18/16/14/13/10/9: Pro has h1..h5 + body + body_small — map approximately; exact 48/9 may need extra bins later.
- Icons: generic set (power, menu, battery, wifi/signal, heart, chevrons, check, refresh, play/pause).
  MISSING dedicated: scan, rfid, spo2, rr, bp, logo, start, restart, abort, select, monitor, reset, stop, link.
  T5 cannot be fully closed without Figma download_assets for those glyphs — reuse nearest generic icons as temporary stand-ins only if screens must ship; prefer real Figma assets.
- globals.xml still has Pro default purple accent (0x9429FF) and dark_bg 0x12151C — NOT Figma triage tokens (#0d1329, #00d460).
  T2 (dark tokens) remains HUMAN: edit globals.xml in Editor to Figma tokens, then re-export.

## Task 6 — bare sim without complete Pro export (2026-07-28)

- Gate `HAS_UI` / `lib-ui` on **both** `ui/CMakeLists.txt` **and** `ui/ui.h`. Partial Pro export has CMakeLists but missing `ui.h`/`ui.c`/`globals_gen.*`; linking still fails.
- `sim/src/main.c` already has `#else` welcome label path when `HAS_UI` is unset — no main.c change needed.
- Window stays 480×480 via existing `hal_init(480, 480)`.
- macOS has no GNU `timeout`; use `./bin & sleep 5; kill $pid` and treat kill-after-5s as exit 124 for acceptance.
- `grep SDL3|lv_drivers sim/` must exclude `sim/build` / `_deps`: LVGL v9's own `src/drivers/lv_drivers.h` is not the external `lv_drivers` package. Project sources use SDL2 only.

## Task 24 — ui_nav + ui_action
- LVGL v9.5 key codes (from lv_group.h): PREV=11, NEXT=9, ENTER=10, ESC=27. PREV is NOT 8 (that is BACKSPACE).
- ui_action_on_key mirrors those codes so nav/action stays LVGL-free for host `cc -c` selftests.
- Physical ButtonBar indices 0..3 map via ui_input to PREV/NEXT/ENTER/ESC; on_key reverses that mapping, so Age/Gender Up/Down/Back/Select stay btn 0/1/2/3.
- Screen show is a function-pointer registry (`ui_nav_register`) — stubs until Pro export fills create/load.
- Non-button transitions: `ui_nav_on_rfid_ready` (Scanning→Berhasil) and `ui_nav_on_measure_done` (Mengukur→Result).
- `ui_nav_go(HOME|SCANNING)` resets session; RESULT/BERHASIL/etc. do not.

## Task 25 (partial) — ui_runtime (no Pro screens)
- `ui_runtime_tick(now_ms)` is pure glue: mock_tick → screen-aware session fill → nav transitions. No LVGL, no sleep.
- Screen enter detection: `s_prev_screen` vs `ui_nav_current()`. SCANNING auto-starts mock scan; MENGUKUR auto-starts measure once (`s_measure_started` reset on leave).
- Measure done path: set mock priority into session first, then `ui_nav_on_measure_done()` so RESULT has priority ready.
- RESULT only fills priority if `!ui_session_has_priority()` (one-shot); MONITOR only refreshes vitals jitter.
- `ui_runtime_debug_cycle_priority()` cycles mock AND rewrites session when priority already set (QA Result variants without re-measure).
- Selftest: `ui/logic/ui_runtime_selftest.c` — host `cc -Iui/logic` links runtime+mock+nav+session+action. UI_MEASURE_MS=2000; tick at 0/500/1000/2000/2500.
- Full plan Task 25 (LVGL setters / screenshots) deferred until Pro-generated screens exist.

## Task 28 — docs match current reality
- AGENTS.md Code conventions rewritten: layout ui/ + ui/logic/ + sim/ + main/; no main/ui/screens hand modules; *_gen no-edit; HAS_UI needs ui/ui.h; button map + ui_action; logic done / screens human.
- Build section: source ~/.espressif/v6.0.2/esp-idf/export.sh (verified v6.0.2).
- docs/ui-workflow.md created: open ui/, edit/export full project, artifact checklist, sim cmake path to bin/lvgl_simulator, idf export+build, three host selftest cc lines, known blockers (partial export, tokens, icons, no CLI).
- Hardware traps + MQTT contract sections left intact.
- Evidence: .sisyphus/evidence/task-28-agents.txt, task-28-workflow-doc.txt
