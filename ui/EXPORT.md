# ui/ export policy (T23)

Full C export is **human-only**: LVGL Pro Editor, open folder `ui/`, **Ctrl+B** (Compile & export code). Pro CLI is not licensed here. Agents must **not** invent fake `*_gen` files.

Generated sources live **beside** XML under `ui/` (not `ui/generated/`). Never hand-edit `*_gen.c` / `*_gen.h`. Fix XML, re-export the whole project, commit the result.

## Required after full export

### Project glue

| Artifact | Notes |
| --- | --- |
| `ui/ui.h` | Required for sim `HAS_UI` |
| `ui/ui.c` | Project entry |
| `ui/ui_gen.h`, `ui/ui_gen.c` | Must declare/create triage screens |
| `ui/globals_gen.*` | If Editor emits them |
| `ui/file_list_gen.cmake` | **Must list every row below** |
| `ui/component_lib_list_gen.cmake` | If Editor emits it |
| `ui/CMakeLists.txt` | Updated by Editor if needed |

### Screens (8) — XML already present

| XML | Required gen |
| --- | --- |
| `screens/home.xml` | `home_gen.c`, `home_gen.h` |
| `screens/scanning.xml` | `scanning_gen.c`, `scanning_gen.h` |
| `screens/berhasil.xml` | `berhasil_gen.c`, `berhasil_gen.h` |
| `screens/age.xml` | `age_gen.c`, `age_gen.h` |
| `screens/gender.xml` | `gender_gen.c`, `gender_gen.h` |
| `screens/mengukur.xml` | `mengukur_gen.c`, `mengukur_gen.h` |
| `screens/result.xml` | `result_gen.c`, `result_gen.h` |
| `screens/monitor.xml` | `monitor_gen.c`, `monitor_gen.h` |

Typical create APIs (names may match Editor conventions): `home_create()`, `scanning_create()`, … Used as `lv_screen_load(home_create());` in sim after export.

### Triage components (3) — XML already present

| XML | Required gen |
| --- | --- |
| `components/triage/status_bar.xml` | `status_bar_gen.c` / `.h` |
| `components/triage/button_bar.xml` | `button_bar_gen.c` / `.h` |
| `components/triage/vital_card.xml` | `vital_card_gen.c` / `.h` |

Exact path under `ui/components/triage/` is preferred; Editor may place gens next to XML.

### Also keep / re-export

- **base_box no-scroll**: `components/layout/base_box/` stays `scrollable="false"` (XML + regenerated `base_box_gen.*`).
- **Battery assets**: `images/battery.png`, `battery-full.png`, `battery-medium.png` and any emitted `battery*_data.c` / `icon_battery*_data.c` listed in `file_list_gen.cmake` when used.

## Never hand-edit rule

1. Do not edit `*_gen.c` / `*_gen.h` / generated `file_list_gen.cmake` by hand to "add" screens.
2. Do not commit hand-written triage gens pretending export ran.
3. Partial export (CMakeLists only, or template-only gens) is a failure mode. Full project only.

## Snapshot before human export (2026-07-29)

**Present (template export):** Pro layout/controls/list/typography gens, `screen_components_gen`, fonts, generic icons, `ui.h`/`ui.c`/`ui_gen.*`, `file_list_gen.cmake` (template only).

**Missing (blocks T23 complete):**

```
home_gen.*
scanning_gen.*
berhasil_gen.*
age_gen.*
gender_gen.*
mengukur_gen.*
result_gen.*
monitor_gen.*
status_bar_gen.*
button_bar_gen.*
vital_card_gen.*
```

`file_list_gen.cmake` currently has **zero** of those paths. `globals_gen.*` also absent.

## Verify commands (after user exports)

```bash
set -e
test -f ui/ui.h
grep -E 'home_gen|scanning_gen|berhasil_gen|/age_gen|gender_gen|mengukur_gen|result_gen|monitor_gen' ui/file_list_gen.cmake | wc -l   # expect 8 paths
grep -E 'status_bar_gen|button_bar_gen|vital_card_gen' ui/file_list_gen.cmake | wc -l   # expect 3
cmake -B sim/build -S sim && cmake --build sim/build
```

Then load Home in `sim/src/main.c` and run `./sim/build/bin/lvgl_simulator` from repo root.

Gate evidence: `.sisyphus/evidence/task-23-export-gate-pending.txt`.
