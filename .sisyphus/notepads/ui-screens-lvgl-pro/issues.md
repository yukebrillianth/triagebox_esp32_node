
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
