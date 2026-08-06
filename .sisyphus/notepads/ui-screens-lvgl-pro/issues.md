
## [2026-07-28] Simulator restructure verification
- `cmake -B sim/build -S sim` succeeds and finds sibling `sim/../ui`; simulator include/run paths now use the repository root so `#include "ui/ui.h"` and `A:ui` resolve correctly once the export is complete.
- Full re-export from LVGL Pro Editor is required: `ui/ui.h` and `ui/ui.c` are missing, so `lvgl_simulator` cannot compile/link. Do not hand-write them.
- `lib-ui` currently stops in generated sources after compiling LVGL and the icon/image list because the partial export also lacks Editor-generated globals such as design constants/styles/subjects/font aliases. Exact evidence: `.sisyphus/evidence/task-6-sim-restructure-libui-build.txt`. This is not a restructure path error.
- `ui/lvgl_open_template.h`, `ui/file_list_gen.cmake`, and `ui/component_lib_list_gen.cmake` are temporary stubs and must be deleted/replaced when the real full Editor export lands.

## [2026-07-28T16:09Z] Root cause of T8/T23 export blocker — NEEDS USER ACTION
Confirmed root cause of the lib-ui compile failure: the copied "lvgl_vscode_project" export from
LVGL Pro Editor is a PARTIAL export. Missing:
  1. ui/ui.h, ui/ui.c (top-level init/create API - src/main.c needs this)
  2. globals_gen.c / globals_gen.h — the compiled form of ui/globals.xml's <consts> (RADIUS_DEFAULT,
     SPACE_LG, COLOR_ACCENT, COLOR_LIGHT_TEXT, OPA_MUTED, etc — dozens of symbols every *_gen.c uses)
  3. ui/file_list_gen.cmake, ui/component_lib_list_gen.cmake (CMake glue) — currently TEMP STUBS
  4. ui/lvgl_open_template.h — currently a TEMP STUB (hand-written, functionally adequate for single-target)

This is NOT a restructure/path bug — confirmed via full build log (task-6-sim-restructure-libui-build.txt).
LVGL v9.5.0 fetch + SDL2 + all icons/fonts/24 components compile fine; only the pieces the Editor
normally auto-generates on a FULL project export are missing.

**ACTION NEEDED FROM USER**: In LVGL Pro Editor, use the full "Compile & export code" / "Generate VSCode
Project" on the WHOLE ui/ project (not per-file), then copy the refreshed output over ui/ again. This
will produce globals_gen.c/h + ui.h/ui.c + the two cmake list files, which supersede our temp stubs.
Until then, Task 8 (export pipeline proof) and Task 23 cannot complete, and Tasks 9-11/15-22 (screen XML
authoring) can proceed in the Editor but won't be build-verifiable until re-export happens.

Temp stub files to DELETE once real export lands: ui/lvgl_open_template.h, ui/file_list_gen.cmake,
ui/component_lib_list_gen.cmake.
## Task 5 — deferred Figma assets (2026-07-29)
- Full Figma `download_assets` is still needed for the final logo, RFID, SpO2, RR, BP, and warning assets. The generic aliases in `ui/logic/ui_icons.h` are temporary; no binary PNG assets were invented. This download is optional later when Figma MCP asset access is available.

## Task 15-22 screens (2026-07-29)
- Dedicated Figma logo / RFID / SpO2 / RR / BP icons still mapped to generic Lucide stand-ins (icon_heart, icon_search, icon_signal). Visual fidelity gap until Figma assets land.
- Monitor Figma uses tinted gradient vital cards; XML uses plain vital_card + size styles (dark-first simplification).
- Full Pro export still required for runtime proof (Task 23).

## globals Figma Flow clash (2026-07-29)
- Parallel Figma Flow dumps (default.xml, scanning_rfid.xml, …) + Flow globals overwrite conflict with agent Pro token system. Keep agent tokens outside fences. Full Editor re-export may still need reconciliation of typography component fonts (font_h1 etc).

## Task 15 remaining export gaps (2026-07-29)
- Full LVGL Pro Editor export is still required before Home/StatusBar/ButtonBar can be rendered in sim or linked as generated C.
- Current Lucide nav source PNGs are generally 16×16; they are correctly centered in 24×24 boxes, but pixel-identical Figma glyphs require exporting the true 24×24 Figma assets later.
- Runtime status application remains Task 26: bare image-name strings are not valid `lv_image_set_src` descriptors in exported C, so wire generated battery descriptors and named object handles only after the full export exists.

## [2026-07-29] Scrollbar false positive as white border
- User saw white lines on ButtonBar; root cause was scrollbar not theme border.
- Re-export in Editor needed for generated base_box_gen.c to pick up scrollable=false (XML source fixed; gen may lag until Compile & export).

## [2026-07-29] T23 BLOCKED — human Editor export
- file_list_gen.cmake has no home/scanning/...*_gen; only template + screen_components.
- User must Ctrl+B full project export in LVGL Pro Editor, then agent can verify builds + wire sim.

## 2026-07-29 vital icon colors
- Resolved: all vital icons green via forced #color_accent recolor.

## T26 gotchas
- Missing trailing slash on asset path silently breaks all tiny_ttf loads.
- Undefined LV_USE_XML skips permanent screen creation (Editor export assumes the macro exists as 0 or 1).
