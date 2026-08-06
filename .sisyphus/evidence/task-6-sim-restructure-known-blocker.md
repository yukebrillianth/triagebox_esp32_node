# Task 6 simulator restructure — known export blocker

CMake configuration succeeds from the new `sim/` project root and finds the sibling UI project at `sim/../ui`.

The full `lvgl_simulator` executable cannot compile/link yet because `ui/ui.h` and `ui/ui.c` are missing from this partial LVGL Pro Editor export. `sim/src/main.c` includes `ui/ui.h` and calls `ui_init("A:ui")` when the UI library is present. Re-exporting the full project from LVGL Pro Editor (not individual generated files) must provide those files; they were deliberately not fabricated here.

The requested `lib-ui` build also does **not** currently complete. It compiles LVGL v9.5.0, all listed icon/image sources, and reaches generated UI component/screen sources, then fails because this partial export is missing additional global Editor-generated declarations/definitions (for example `RADIUS_DEFAULT`, `COLOR_LIGHT_TEXT`, `COLOR_DARK_TEXT`, `COLOR_ACCENT`, `SPACE_LG`, styles, subjects, and font aliases). The exact compiler output and exit code are in `task-6-sim-restructure-libui-build.txt`. This is an export-completeness blocker, not a path failure caused by moving `ui/` beside `sim/`.

Temporary probes remain in place: `ui/lvgl_open_template.h`, `ui/file_list_gen.cmake`, and `ui/component_lib_list_gen.cmake`. Delete/replace them when a complete Editor export lands.
