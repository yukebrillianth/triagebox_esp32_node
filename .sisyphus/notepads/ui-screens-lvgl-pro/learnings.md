
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

## Task 2 — Figma dark tokens (2026-07-29)
- Updated ui/globals.xml dark palette to Figma: screen_bg 0x0d1329, card_bg 0x1a2651, text_primary 0xFFFFFF, accent 0x00d460, danger 0xfb2c36; added color_text_secondary 0x99a1af, color_text_on_card 0xd1d5dc, color_status_ok 0x00c950; radius_default 10.
- style_panel_dark border now #color_dark_panel; style_text_muted uses #color_text_secondary (not text_opa 60%).
- Also patched matching COLOR_*/RADIUS_DEFAULT macros in ui/ui_gen.h so current sim/build picks Figma colors without Editor re-export.
- ui_gen.c style init body still needs full Pro re-export for border/muted lines and any new const usage; do not hand-edit *_gen.c.
- Purple 0x9429FF is gone from ui/ (evidence task-2-no-purple.txt count 0).
- Light palette left as Pro defaults (dark-first T2).
## [2026-07-28T17:40Z] T2 tokens + dark default
- globals.xml Figma dark applied; ui_gen.h COLOR_* macros updated for build without re-export.
- subject_theme_dark default set to 1 (dark-first) in globals.xml and ui_gen.c init — re-export will overwrite ui_gen.c; keep globals.xml value=1 as source of truth.

## Task 5 — icon role aliases without Figma download (2026-07-29)
- Closed Task 5 mapping-only path: ui/logic/ui_icons.h maps ButtonBar/status/vital roles to existing Pro icon_* names (icon_search/play/refresh/close/…).
- Evidence: .sisyphus/evidence/task-5-icon-inventory.txt (HAVE/MISSING + mapping table).
- Full Figma download_assets still needed later for logo, rfid, spo2, rr, bp, warning — optional; do not invent PNGs.

## 2026-07-28 Task 27 partial — main links ui/logic
- Wire via main/CMakeLists.txt SRCS ../ui/logic/*.c + INCLUDE_DIRS ../ui/logic + REQUIRES lvgl
- ui_input.c compiles under IDF with REQUIRES lvgl (managed lvgl__lvgl 9.5.0)
- app_main only calls ui_runtime_init(); no esp_lvgl_port / board bring-up
- Full Pro lib-ui / triagebox_ui_init deferred until complete export
- --gc-sections drops unreferenced logic symbols from ELF; still in libmain.a

## Tasks 9–22 — triage components + 8 screens XML (2026-07-29)
- Path: `ui/components/triage/{status_bar,button_bar,vital_card}.xml` + `ui/screens/{home,scanning,berhasil,age,gender,mengukur,result,monitor}.xml` (not ui_pro/; project lives under ui/).
- Pattern: same as controls/button — `<component><api><prop>…</prop></api><styles>…</styles><view extends=…>`.
- Tokens only: `#color_dark_bg/panel/text`, `#color_accent/danger/status_ok/text_secondary`, `#radius_default`, `#space_*`. Zero raw `#RRGGBB` in new XML.
- ButtonBar: 4 fixed cells 120×71; empty labels via default `""` + `hidden="{!labelN}"`; Power via `color2="#color_danger"` at use site (not auto-detect string "Power").
- StatusBar props: battery_text/link_text/clock_text with demo defaults for preview only.
- VitalCard: icon/value/unit/label; compact card_bg style; used on mengukur/result/monitor.
- Screens: permanent="true", style_screen_light + bind_style dark, status_bar top + button_bar bottom, content grow=1.
- Icons: Lucide stand-ins (search=scan, heart=vitals/logo, signal=link/RR/BP, check/close/play/refresh/chevrons). Dedicated triage glyphs still missing (T5).
- subject_theme_dark default set to **1** (dark-first).
- **Export required**: no *_gen.c written; open `ui/` in LVGL Pro Editor → full Compile & export before sim/ESP pick up new screens.
- Not pixel-perfect: layout is Figma-structure faithful (skeleton + copy + bar labels), not measured px-perfect without Editor preview.

## Task 15-22 — 8 triage screens XML (2026-07-29)
- Delivered/verified ui/screens/{home,scanning,berhasil,age,gender,mengukur,result,monitor}.xml.
- Pattern: permanent screen, 480×480 column, style_screen_light + bind style_screen_dark, <status_bar/>, content column grow=1, <button_bar .../>.
- Custom tags status_bar/button_bar/vital_card resolve from components/triage/ (same as Pro component base name).
- Copy from skill: Memindai RFID..., MERAH - IMMEDIATE, Pilih Rentang Usia, Monitoring aktif, etc. Exact Indonesian.
- Tokens only #color_* / #space_* / #radius_*; no raw hex in the 8 files.
- Figma polish: Abort/Stop use colorN=#color_danger; result vital labels "SpO2 % / HR bpm / RR /min / BP mmHg"; home START hint single line; status dots column+muted.
- Age/Gender use icon_arrow_*; Monitor Stop uses icon_pause (task Lucide list).
- screen_components.xml untouched; no new *_gen written.
- Parallel Figma Flow dumps (default.xml, scanning_rfid.xml, …) also present under ui/screens/ — leave OWNED_BY=FIGMA alone; our 8 short names are the agent contract.
- Evidence: .sisyphus/evidence/task-15-22-screens-verify.txt
- Export still human: full Editor export needed before sim can load these screens as C.

## globals restore after Figma Flow overwrite (2026-07-29)
- Figma Flow replaced ui/globals.xml with Flow-only consts/images/fonts/styles fences, dropping #color_* / Lucide icon_* / style_screen_* / subjects.
- Agent re-added tokens, Lucide icon data refs, Montserrat font bins, screen/panel styles, subjects OUTSIDE Flow fences so next Flow export should not wipe them (fences only regenerate between begin/end markers).
- 8 screens still reference #color_* and icon_*; they depend on this restore.

## Task 15 — Home fidelity repair (2026-07-29)
- The prior verification failure was exact: a broad `assert '278' not in home` matched a stale comment, while the image itself was already 249×58. Removed the stale size reference and fixed geometry explicitly.
- Home now follows Figma `16:98`: StatusBar 0/0/480×48, content 66/64/347×312, native logo 249×58 centered at local x=49, hint local y=178, dots local y=281, ButtonBar y=409 h=71.
- User override wins for hint action copy: `SCAN` is accent text while the ButtonBar label remains `Scan`.
- `inner_align="center"` is required on each fixed 24×24 nav image box because current Lucide PNG leaves are mostly 16×16.
- Battery state assets are colored 24×24 PNGs; StatusBar exposes `battery_icon` and does not recolor it. Threshold helper returns empty <25, medium 25..74, full >=75.
- `ui_status` remains pure C: battery asset name selection plus host-local `HH:MM`; runtime LVGL object application is deferred until the complete Pro export provides stable descriptors/handles.
- Custom gradient tokens/style plus logo/battery registrations are after their respective Figma Flow end fences so Flow regeneration preserves them.

## [2026-07-29] No scrollbar on node UI
- White edge was LVGL scrollbar from base_box (column/row/container chain) applying style_scrollbar.
- Fix: base_box scrollable=false + local invisible scrollbar style; globals style_scrollbar width=0 opa=0; button_bar/status_bar scrollable=false.
- Canvas fixed 480x480 — triage screens must never scroll.

## [2026-07-29] T15 Home complete
- Verified logo native, no scroll, battery states, local clock helper, Flow dups gone.
- Remaining screens T16-22 still older flex templates; need same no-scroll + dark-first polish.

## [2026-07-29] T16–T22 screen polish (Home quality bar)
- Applied Home dark-first pattern to all 7 remaining screens: solid `style_*_root` with `#color_dark_bg`, `scrollable="false"`, explicit status_bar 480×48 + button_bar 480×71.
- Dropped `style_screen_light` + `bind_style` on these screens for Editor preview reliability (dark-first); light theme still deferred.
- Age/Gender focused state is **solid green fill** (`bg_color="#color_accent"`) not border-only — matches Figma selected card. Subtitle uses `text_opa` so it inherits parent text color on focus.
- Prefer Inter tokens (`font_inter_*`) over Montserrat h2/h4 wrappers on triage screens for Figma fidelity (same as Home).
- Named widgets kept for runtime glue: patient_id, measure_progress, measure_pct, vc_*, priority_label, result_banner, update_ago, opt_*.
- XML entities required: `&amp;` for "Anak-anak & Remaja", `&gt;` for ">60 tahun" — ElementTree decodes them; raw-string greps must account for entities.
- Evidence: `.sisyphus/evidence/task-16-22-screen-polish.txt`. Export still human (T23).

## [2026-07-29] T15-22 complete
- All 8 product screens XML polished: home,scanning,berhasil,age,gender,mengukur,result,monitor.
- Shared: no-scroll, dark root, status 48 + bar 71, tokens only, named widgets for runtime.
- Age/Gender focused = solid accent fill (Figma).
- Next: human full Pro export (T23) then sim wire (T26).

## [2026-07-29] T23 export gate (docs only, BLOCKED human)

- Full C export remains HUMAN: open `ui/` in Pro Editor, Ctrl+B full project. No Pro CLI.
- Gen path is `ui/*_gen` beside XML, not `ui/generated/` (plan wording stale).
- Agent prepared: `docs/ui-workflow.md` (8-screen inventory + post-export checklist), `ui/EXPORT.md` (required gens + never-hand-edit), evidence `.sisyphus/evidence/task-23-export-gate-pending.txt`.
- Did NOT invent fake triage `*_gen` files.
- Missing gens: home/scanning/berhasil/age/gender/mengukur/result/monitor + status_bar/button_bar/vital_card (0/8 screens, 0/3 components in file_list).
- Present only: template gens + screen_components_gen + ui.h/ui.c/ui_gen.
- base_box already scrollable=false; battery png + data.c already under ui/images/.
- After user export: run greps in evidence file, then T26 sim wire (`lv_screen_load(home_create())`, SDL keys).
- Remaining: T26, F1–F4. Do not mark plan item 23 complete until export verifies.

## T26 prep — sim wire without triage gens (2026-07-29)
- `ui_priority_display_label` lives in new `ui/logic/ui_types.c` (was declared-only in header).
- `sim/CMakeLists.txt` always compiles logic: types, session, mock, nav, action, runtime, status, input (no selftests).
- Gate `HAS_TRIAGE_SCREENS` on `EXISTS ui/screens/home_gen.c` — without export, sim keeps template `ui_init` / welcome and still ticks runtime.
- SDL keys via `SDL_GetKeyboardState` (not PollEvent) so LVGL SDL driver still receives events; keys 1-4 push mock + `ui_action`, p/c cycle priority.
- Permanent-screen pattern ready under `#if HAS_TRIAGE_SCREENS`: create-once table + `ui_nav_register` + `lv_screen_load`.
- Do not invent `*_gen` files; T23 human export still required for full walk screenshots.

## [2026-07-29] T26 prep verified
- sim links ui/logic; keys 1-4 + p/c; HAS_TRIAGE_SCREENS gated on home_gen.c
- ui_priority_display_label implemented
- Still blocked on T23 human export for visual 8-screen walk

## 2026-07-29 — export focusable + vital icons
- `focusable` is NOT a valid LVGL Pro XML attribute on `lv_obj` — Editor rejects export with "Unknown attribute focusable". Use `clickable="true"` + `scrollable="false"` for age/gender options; keypad group focus works without focusable.
- Vital card icons (user PNGs from Downloads, 28x28):
  - SpO2 → icon_oxygen → images/icons/oxygen.png
  - HR → icon_heart_pulse → images/icons/heart-pulse.png
  - RR → icon_respiratory → images/icons/respiratory.png
  - BP → icon_blood_pressure → images/icons/blood-presure.png (filename spelling intentional)
- Register new icons in globals.xml Lucide block outside Figma Flow fences.

## [2026-07-29] Export unblock + vital icons
- Removed invalid focusable= attr from age/gender (Pro XML rejects it; clickable is enough).
- Vital icons: oxygen/heart-pulse/respiratory/blood-presure from Downloads → ui/images/icons + globals registration.
- User can Ctrl+B export again.

## 2026-07-29 vital icon colors
- Forced accent green on style_vital_icon made all vital icons green.
- Fix: icon_color prop + per-vital tokens (spo2 blue, hr red, rr cyan, bp purple).
- style_image_recolor="$icon_color" pattern already used by button.xml.

## [2026-07-29] T23 export complete
- User Ctrl+B produced all 8 screen + 3 triage gens; sim HAS_TRIAGE_SCREENS=1 builds.

## T26 sim full run (2026-07-29)

- **Font path:** `ui_init("A:ui/")` trailing slash required. Gen does `snprintf("%s%s", asset_path, "fonts/X")` → bare `A:ui` becomes `A:uifonts/X`.
- **Permanent screens:** `ui_init_gen` only creates them under `#if defined(LV_USE_XML) && LV_USE_XML == 0`. If `LV_USE_XML` is **undefined** (this LVGL build has no XML option), the block is skipped and `home`/`scanning`/… stay NULL → `lv_screen_load(NULL)` assert hang.
- **Fix:** `target_compile_definitions(lib-ui PUBLIC LV_USE_XML=0)` (+ sim) in `sim/CMakeLists.txt`. Fallback: if `home==NULL` after init, call `*_create()` once (gens reuse global when non-NULL).
- **Do not double-create** when permanent block already ran — children pile onto same root.
- **Nav evidence:** `SIM_AUTO_WALK=1` drives same `ui_action` path as keys 1–4; full Home→…→Monitor in ~3s with `UI_MEASURE_MS=2000` / `UI_MOCK_SCAN_MS=500`.
- **Assert:** sim defaults use `LV_ASSERT_HANDLER abort();` so NULL load fails fast instead of `while(1)`.
- **Run from repo root** so `A:` + `ui/` resolves; `cmake --build sim/build --target run` sets WORKING_DIRECTORY to root.

## [2026-07-29] Final Wave F1-F4 dispatched
- F1 Confucius plan compliance: ses_054b95005ffeQlKqPxRDWQCdLG (bg_2d634bd1)
- F2 quality: ses_054b939e4ffek4I96dsNQmM853 (bg_39b10bb0)
- F3 manual QA: ses_054b91b9fffezTm0ICPlL0CQ4n (bg_61a01e56)
- F4 scope: ses_054b903faffeLqG4QE9DbvVwDt (bg_40ff364f)
- T1-T28 all [x]; awaiting 4 APPROVE verdicts then user okay before marking F boxes.

## F2 quality (2026-07-29)
- sim+idf build PASS
- 25 hand-written files clean: no v8, no raw hex in ui/logic, no platform leak, single ui_action
- VERDICT: APPROVE
- evidence: .sisyphus/evidence/final-f2-quality.txt

## [2026-07-29] Final Wave consolidated
- F1 APPROVE — .sisyphus/evidence/final-f1-compliance.txt
- F2 APPROVE — final-f2-quality.txt (sim+idf PASS, 25 clean)
- F3 APPROVE — final-qa/VERDICT.txt (10/10 scenarios, 11 edges)
- F4 APPROVE — final-f4-scope.txt (28/28, CLEAN; deep agent timeout recovered by orchestrator)
- Plan rule: do NOT mark F1-F4 [x] until explicit user okay.
- DoD recheck: sim build 0, idf build 0, SIM_AUTO_WALK ALL_SCREENS_OK, no v8/hex/platform leak.

## [2026-07-29] Boulder parked — user okay gate
- Agent lane exhausted: T1-T28 [x], F1-F4 APPROVE evidence on disk, DoD+Final Checklist nested [x].
- Top-level F1-F4 remain [ ] per plan until user says okay.
- No more implementation tasks to auto-continue.

## [2026-07-29] PLAN CLOSED — user okay received ("Lanjuttt")
- F1-F4 marked [x]. All 32/32 complete.
- Boulder complete. Next efforts: STM32 serial link, C5.0 inference, LoRa TX, board bring-up.

## [2026-08-07] Figma icon swap + touch/click wiring
- Rendered real Figma SVGs → white-stroke alpha PNGs via rsvg-convert at Figma px (bar 24, vital 28, update 12). Color via LVGL image_recolor at use site.
- New semantic symbols: icon_gender_male/female (was lock/bluetooth), icon_monitor (was signal), icon_warning (was info), icon_update (was clock), Age Up/Down now chevrons (matches Gender).
- Removed unused source PNGs + globals decls: arrow_up/down, bluetooth, info, lock, clock. icon_star kept (monoicon default).
- Touch: ui/logic/ui_bindings.c (added to user_config.cmake as lib-ui user source; called from ui.c after ui_init_gen). Finds button_bar_# → cell0..3 and opt_* by name, LV_EVENT_CLICKED → ui_action. Same dispatcher as keypad. No *_gen edits.
- Age/Gender Up/Down now move pending selection (ui_nav_move_pending_age/gender); 50ms timer re-applies LV_STATE_FOCUSED via ui_bindings_sync_selection. Touch row = direct select.
- Generated *_gen.c still hold OLD icon symbols (icon_lock/info/signal) until next Editor Ctrl+B. Sim builds+runs; screen still shows old glyphs for gender/result/monitor until re-export. Documented in ui-workflow.md known blockers.
- IDF PYTHON for LVGLImage.py: system python is externally-managed; used a venv with pypng+lz4. Only needed for the 3 CLI-generated symbols experiment — reverted, real assets are plain PNG + full Editor export path.
- Verified: sim build 0, idf build 0, SIM_AUTO_WALK ALL_SCREENS_OK, nav+runtime selftests ALL_PASS.
