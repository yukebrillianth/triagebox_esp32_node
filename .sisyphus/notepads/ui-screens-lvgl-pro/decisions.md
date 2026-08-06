
## [2026-07-28T15:31Z] Session start — atlas
- git repo initialized (was not a git repo). Initial commit 7f3272a with README/AGENTS.md/plan/skills.
- Toolchain check: cmake present (homebrew), sdl2-config present (2.32.10). idf.py NOT FOUND, IDF_PATH unset.
  -> T7 (ESP-IDF skeleton) and the idf.py-build half of T8/T27 are BLOCKED until ESP-IDF is installed+exported.
  -> Sim path (cmake+SDL2) is fully available now. Proceeding agent-lane logic + sim tasks first.
- Tasks 1, 2, 9, 10, 11, 15-22 require the user in LVGL Pro Editor (GUI, human-owned per plan's "Execution Model" section). Not executable by agent.
- Execution order chosen: start with T3 (types) since it unblocks T12/T13/T14, and T6 (sim skeleton) in parallel. Then T4/T5 (assets). T7 deferred pending idf.py availability - will attempt install or flag to user.

## [2026-07-28T16:13Z] ESP-IDF now available — T7 unblocked
- User installed ESP-IDF. Located at: ~/.espressif/v6.0.2/esp-idf  (ESP-IDF **v6.0.2**)
- First `export.sh` FAILED: python venv "idf6.0_py3.14_env" did not exist (system python is 3.14.6).
  Fixed by running `~/.espressif/v6.0.2/esp-idf/install.sh esp32s3` which created the venv + toolchain.
- Verified working: `source ~/.espressif/v6.0.2/esp-idf/export.sh` then `idf.py --version` -> "ESP-IDF v6.0.2".
- **EVERY shell that needs idf.py MUST source that export.sh first** (it is not on PATH by default).
- NOTE: plan/AGENTS.md assumed ESP-IDF >=5.3.1 (CI 5.5.x/6.0.x). v6.0.2 is within the supported range.
  Watch for BSP compat: waveshare/esp32_s3_touch_lcd_4 ^3.0.0 + esp_lvgl_port ^2.8.0 declare idf >=5.2,
  should be fine on 6.0.2 but if the component manager complains, that's the cause.

## [2026-07-28T16:50Z] T1 verified, T4 closed with Pro fonts
- T1: ui/project.xml present, 480x480, lvgl 9.5.0 — mark complete (user-generated VSCode/Pro project already on disk).
- T4: Use Pro Montserrat compiled bins (ui/fonts/*_data.c) as Inter substitute. Symlink ui/assets/fonts -> ../fonts for plan path.
  Exact px sizes differ slightly from Figma (h1=44 vs 48); acceptable for v1; regenerate bins if pixel QA fails.
- T2 still HUMAN: replace Pro purple/dark defaults in globals.xml with Figma triage tokens.
- T5 inventory: many triage-specific icons missing; needs Figma download_assets before closing.

## [2026-07-28T17:20Z] T8 unblocked — full export landed
- ui/ui.h + ui/ui.c + ui_gen.c/h now present (user/Editor re-export or writing-agent side effect).
- Tokens/styles live inside ui_gen.h (#define SPACE_LG, RADIUS_DEFAULT, colors) — no separate globals_gen.* required for this template.
- sim CMake HAS_UI gate opens; lib-ui links; lvgl_simulator runs 3s OK with real UI lib.
- Still only template screen_components, NOT the 8 triage screens.
- T2 tokens still Pro purple defaults in globals.xml — human must retoken to Figma.
- T5 icons still missing triage-specific assets.
- T9-22 still human Editor work.
- T26/T27 need screens + register show fns in runtime before full nav on device/sim UI.

## 2026-07-28 Task 27
- Plan wants triagebox_ui_init + full ui/generated; partial delivery is logic-only link-check (orchestrator approach)
- sdkconfig still has CONFIG_SPIRAM not set despite sdkconfig.defaults — out of scope for link-check

## T26 prep decisions
- Direct `ui_action` on key edge in sim (in addition to mock push for keypad indev) so ButtonBar works without focus group / gen screens.
- Single-slot mock button buffer: press then release on next edge is enough for indev; action fires on press only.

## 2026-07-29 vital icon colors
- Per-vital icon colors live as named tokens in globals.xml; screens pass icon_color, not raw hex.
