# UI workflow: LVGL Pro + logic + sim + ESP-IDF

How to author screens, export C, run the desktop sim, build firmware, and check the hand-written logic layer.

**Current state (post T15–22):** all 8 triage screens and 3 triage components exist as **XML**. Hand-written logic is done. **Full C export is still a human Editor step (T23).** Do not invent or hand-write `*_gen` for triage screens.

Canonical export notes live in **`ui/EXPORT.md`**. Gen files sit **next to** their XML under `ui/` (not a separate `ui/generated/` tree). Plan wording that says `ui/generated/` is stale.

## License and tools

- **LVGL Pro Community** (desktop Editor) is enough for PKM educational use.
- The **Pro CLI is Professional-only and not licensed here.** Never wire `lved-cli` into scripts or CMake. Export is always the Editor GUI button (**Ctrl+B** / hammer / Compile & export code).
- Do not invent a CLI path. Agents and CI consume **committed** exported C only.

## Open the project

1. Install LVGL Pro Editor (Community).
2. Open the folder **`ui/`** (Pro project: `project.xml`, `globals.xml`, `screens/`, `components/`).
3. Canvas target is **480×480**, LVGL **9.5**.

## Current XML inventory (authoritative)

### Screens (`ui/screens/`, T15–22 done as XML)

| File | Flow step | Expected gen after full export |
| --- | --- | --- |
| `home.xml` | Home | `home_gen.c` / `home_gen.h` |
| `scanning.xml` | Scanning RFID | `scanning_gen.*` |
| `berhasil.xml` | Scan Berhasil | `berhasil_gen.*` |
| `age.xml` | Select Age | `age_gen.*` |
| `gender.xml` | Select Gender | `gender_gen.*` |
| `mengukur.xml` | Mengukur | `mengukur_gen.*` |
| `result.xml` | Scan Result | `result_gen.*` |
| `monitor.xml` | Monitor | `monitor_gen.*` |

Template demo screen `screen_components` may still exist; triage flow does not use it.

Shared screen rules (already in XML):

- Root dark style, `scrollable="false"` (no scrollbar on 480×480).
- Status bar 480×48 top, ButtonBar 480×71 bottom.
- Tokens only (no raw hex in logic). Named widgets for runtime glue.

### Triage components (`ui/components/triage/`)

| File | Expected gen after full export |
| --- | --- |
| `status_bar.xml` | `status_bar_gen.c` / `.h` (beside XML under `components/triage/`) |
| `button_bar.xml` | `button_bar_gen.*` |
| `vital_card.xml` | `vital_card_gen.*` |

### Layout / assets that export must preserve

- **`base_box`**: `scrollable="false"` + zero-width scrollbar style in `ui/components/layout/base_box/base_box.xml`. Re-export must keep no-scroll behavior.
- **Battery assets** (status bar): `ui/images/battery.png`, `battery-full.png`, `battery-medium.png` plus any `*_data.c` the Editor emits (`battery_data.c`, `icon_battery_*`, …). Full export must list them in `file_list_gen.cmake` if used by components.

## Edit tokens and screens

1. **Tokens** live in `ui/globals.xml` (Figma dark from `AGENTS.md`: `screen_bg` `#0d1329`, `card_bg` `#1a2651`, `accent` `#00d460`, `danger` `#fb2c36`, …). No raw style hex in `ui/logic/`.
2. Shared triage components under `ui/components/triage/`.
3. The 8 screens under `ui/screens/` (table above). Copy is Bahasa Indonesia, match Figma.
4. Validate layout in the Editor live preview (human).

## Interaction / logic (for the UI developer)

You author XML; the interaction wiring is already hand-written C and stays out of the generated files.

- **One dispatcher.** Every touch cell and every physical/keypad button calls `ui_action(screen, btn_id)` in `ui/logic/ui_action.c`. There is no second code path. To change what a button does, edit that screen's table there.
- **Touch is auto-wired.** `ui/logic/ui_bindings.c` runs from `ui_init()` (via `ui.c`) after the Editor creates the screens. It finds each `button_bar_#` → `cell0..cell3` by name and attaches `LV_EVENT_CLICKED` → `ui_action`. It also wires Age `opt_6_17/18_45/46_60/60_plus` and Gender `opt_male/opt_female` clicks.
- **The contract is the object name.** As long as your XML keeps the ButtonBar cells named `cell0..cell3` and the option rows named `opt_*`, clicking works with zero extra code. Rename a node → update the name list in `ui_bindings.c`.
- **Age/Gender selection.** Up/Down (buttons 0/1) move the pending selection (`ui_nav_move_pending_age/gender`); the focused green card is re-applied every 50 ms by `ui_bindings_sync_selection()`. Select (button 3) commits and navigates. Touch a row directly to select it.
- **Nav map** lives in `ui/logic/ui_action.c` per screen (Abort→Home, Restart→Scanning, Reset→Home, Stop→Result, Back Age→Berhasil, Back Gender→Age, …). Power/Menu are logged no-ops in v1.

So: to make a button "do something", you never touch generated C — edit the action table. To make a new clickable thing, give it a stable `name=` in XML and add one `bind_option(...)` line.

## Icon assets (Figma, pixel-exact)

Real Figma glyphs live under `ui/images/icons/` as monochrome white PNGs at their Figma pixel size (bar/status/gender/monitor/warning = 24×24, vitals = 28×28, update = 12×12). Color comes from LVGL `image_recolor` at the use site, never baked in. Semantic symbols in `globals.xml`:

| Symbol | Role | Symbol | Role |
| --- | --- | --- | --- |
| `icon_search` | Scan | `icon_gender_male` | Male ♂ |
| `icon_close` | Abort | `icon_gender_female` | Female ♀ |
| `icon_play` | Start | `icon_monitor` | Monitor (eye) |
| `icon_refresh` | Restart / Reset | `icon_warning` | Triage warning |
| `icon_chevron_up/down` | Up / Down (Age+Gender) | `icon_update` | Monitor update |
| `icon_arrow_left` | Back | `icon_oxygen/heart_pulse/respiratory/blood_pressure` | Vitals |
| `icon_check` | Select | `icon_power` / `icon_menu` | Power / Menu |
| `icon_pause` | Stop | `icon_signal` | Link status |

Re-exporting an icon: drop a white 24×24 (or Figma-size) PNG into `ui/images/icons/`, add/rename its `<data>` in `globals.xml`, reference it from XML, then full re-export.

## Compile and export code (full project) — human gate T23


1. Open **`ui/`** in LVGL Pro Editor.
2. **Ctrl+B**, hammer icon, or **Compile & export code**.
3. Export the **whole project**, not a single screen. Partial export is a known failure mode.
4. Commit generated C so sim/ESP builds work without the Editor.

### Never hand-edit generated C

- **Never** hand-edit `*_gen.c` / `*_gen.h` (or other Pro-generated sources).
- Fix XML → re-export full project.
- Do **not** invent fake triage `*_gen` files to "complete" T23. Agents stop at docs + verification until a real Editor export lands.

### Required export artifacts (post T15–22)

See **`ui/EXPORT.md`** for the full list. Minimum:

| Artifact | Why |
| --- | --- |
| `ui/ui.h`, `ui/ui.c` | Sim gates `HAS_UI` / `lib-ui` on `ui.h` **and** `ui/CMakeLists.txt` |
| `ui/ui_gen.h`, `ui/ui_gen.c` | Generated project API (must expose `home_create()`, …) |
| `ui/globals_gen.*` | When the Editor emits them (tokens/styles) |
| `ui/file_list_gen.cmake` | Must list **all 8** triage screens + **3** triage components |
| `ui/component_lib_list_gen.cmake` | Component list (if present) |
| 8× `*_gen` screens | `home`, `scanning`, `berhasil`, `age`, `gender`, `mengukur`, `result`, `monitor` |
| 3× triage component gens | `status_bar`, `button_bar`, `vital_card` |

### After export: verify (agent or human)

```bash
# 1) file_list must name every triage screen + component
grep -E 'home_gen|scanning_gen|berhasil_gen|/age_gen|gender_gen|mengukur_gen|result_gen|monitor_gen' ui/file_list_gen.cmake
grep -E 'status_bar_gen|button_bar_gen|vital_card_gen' ui/file_list_gen.cmake

# 2) gen files exist (paths as Editor wrote them)
ls ui/screens/*_gen.c ui/components/triage/*_gen.c 2>/dev/null
find ui -name 'home_gen.c' -o -name 'status_bar_gen.c'

# 3) create APIs present
grep -E 'home_create|scanning_create|berhasil_create|age_create|gender_create|mengukur_create|result_create|monitor_create' ui/ui_gen.h ui/screens/*_gen.h 2>/dev/null

# 4) rebuild sim and load Home
cmake -B sim/build -S sim && cmake --build sim/build
# then in sim/src/main.c under HAS_UI: lv_screen_load(home_create());
./sim/build/bin/lvgl_simulator   # run from repo root
```

Until those greps pass, **T23 stays BLOCKED** on human Editor export. Evidence: `.sisyphus/evidence/task-23-export-gate-pending.txt`.

## Run the SDL simulator

From the repo root:

```bash
cmake -B sim/build -S sim
cmake --build sim/build
./sim/build/bin/lvgl_simulator
```

- Window is **480×480**. Run from repo root (or `run` CMake target) so asset path `A:ui` resolves.
- With `ui/ui.h` + CMakeLists, `HAS_UI=1` links `lib-ui`. Without `ui.h`, welcome fallback only.
- `sim/` always links `ui/logic/` (session, mock, nav, action, runtime, status, types, input). Runtime timer ticks every 50 ms.
- **Triage gens gate:** if `ui/screens/home_gen.c` exists, CMake sets `HAS_TRIAGE_SCREENS=1` and main registers all 8 screens + loads Home. Without it, sim stays on template `ui_init` / welcome and still runs logic.

### Sim key map (T26 prep)

| Key | Effect |
| --- | --- |
| `1` / `2` / `3` / `4` | Physical bar cells 0..3 → `ui_mock_push_button` + `ui_action` (PREV/NEXT/ENTER/ESC map) |
| `p` or `c` | `ui_runtime_debug_cycle_priority` (GREEN→YELLOW→RED→BLACK) |

Typical walk (after triage export): Home `2` (Scan) → wait mock RFID → Berhasil `1` (Start) → Age `4` (Select) → Gender `4` → wait measure → Result `1` (Monitor) → `1` Back → `2` Reset → Home.

Keypad: mock buttons 0..3 → `LV_KEY_PREV` / `NEXT` / `ENTER` / `ESC`. Touch ButtonBar and keys both call **`ui_action(screen, btn_id)`**. See `AGENTS.md`.

## ESP-IDF build

ESP-IDF is not on `PATH` by default. Every shell:

```bash
source ~/.espressif/v6.0.2/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

Verified with **ESP-IDF v6.0.2**. Pin LVGL to v9 in `main/idf_component.yml` (see `AGENTS.md`). Board bring-up (CH32, GT911, PSRAM fb) is out of scope for the screen plan; green `idf.py build` is the bar.

## Logic host selftests

Hand-written logic under `ui/logic/` is platform-neutral and has assert-based selftests (no test framework). From the **repo root**:

### `ui_nav_selftest` (nav + action tables)

```bash
cc -std=c99 -Wall -Wextra -Iui/logic \
  -o /tmp/ui_nav_selftest \
  ui/logic/ui_nav_selftest.c \
  ui/logic/ui_nav.c ui/logic/ui_action.c ui/logic/ui_session.c
/tmp/ui_nav_selftest
```

### `ui_mock_selftest` (RFID / measure / priority / buttons)

```bash
cc -std=c99 -Wall -Wextra -Iui/logic \
  -o /tmp/ui_mock_selftest \
  ui/logic/ui_mock_selftest.c ui/logic/ui_mock.c
/tmp/ui_mock_selftest
```

### `ui_runtime_selftest` (full mock→session→nav tick glue)

```bash
cc -std=c99 -Wall -Wextra -Iui/logic -DUI_MEASURE_MS=2000 \
  -o /tmp/ui_runtime_selftest \
  ui/logic/ui_runtime_selftest.c \
  ui/logic/ui_runtime.c ui/logic/ui_mock.c \
  ui/logic/ui_nav.c ui/logic/ui_action.c ui/logic/ui_session.c
/tmp/ui_runtime_selftest
```

Expect `ALL_PASS` (or the printed screen walk) and exit 0. These do **not** need LVGL or Pro export.

## Add a new screen (checklist)

1. Author XML under `ui/screens/` (reuse status_bar / button_bar / tokens).
2. Full-project **Compile & export code**; confirm new `*_gen.*` and `file_list_gen.cmake` entries.
3. Commit generated C.
4. Wire logic: `ui_nav` show callback, `ui_action` table, `ui_runtime` if needed.
5. Host selftest and/or sim key walk.
6. Rebuild sim + `idf.py build`.

## Known blockers (honest status)

| Blocker | Detail |
| --- | --- |
| **T23 full Pro export (HUMAN)** | XML for 8 screens + 3 triage components is in-tree. `file_list_gen.cmake` still only lists Pro template components + `screen_components_gen`. **No** `home_gen` … `monitor_gen`, **no** `status_bar_gen` / `button_bar_gen` / `vital_card_gen`. Blocked on Editor Ctrl+B. |
| Partial / template-only gen | `ui.h` / `ui.c` / template `*_gen` may exist while triage gens are missing. Do not treat that as T23 complete. |
| `globals_gen.*` | May be absent until a full export that emits them. |
| Missing dedicated triage icons | Resolved: real Figma glyphs are in `ui/images/icons/`. Only battery/wifi state variants remain generic. |
| **Pending re-export after icon swap** | `globals.xml` + screen XML now reference `icon_gender_male/female`, `icon_monitor`, `icon_warning`, `icon_update`, and chevron Up/Down on Age. The committed `*_gen.c` still hold the old symbols until the next Editor **Ctrl+B**. Sim builds and runs meanwhile, but Gender/Result/Monitor keep the old glyphs on screen. |
| Pro CLI | Not available. Export is GUI-only. |
| **T26 sim full walk** | Prep done: logic linked, keys 1-4/`p`/`c`, runtime timer. Full 8-screen walk still needs T23 export (`HAS_TRIAGE_SCREENS`). |
| **F1–F4** | Follow-ups after export (fidelity / light theme / icons / polish as plan defines). |

Logic layer (`ui_types`, `ui_session`, `ui_mock`, `ui_input`, `ui_nav`, `ui_action`, `ui_runtime`) **is** done and host-selftestable. **Screens as XML are authored.** Runtime C for those screens still needs the human full export, then T26.
