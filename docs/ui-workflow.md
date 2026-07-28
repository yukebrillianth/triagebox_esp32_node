# UI workflow: LVGL Pro + logic + sim + ESP-IDF

How to author screens, export C, run the desktop sim, build firmware, and check the hand-written logic layer. Matches the **current** repo state: logic is done; the 8 triage screens and a complete Pro export are still human/Editor work.

## License and tools

- **LVGL Pro Community** (desktop Editor) is enough for PKM educational use.
- The **Pro CLI is Professional-only and not licensed here.** Never wire `lved-cli` into scripts or CMake. Export is always the Editor GUI button.
- Do not invent a CLI path. Agents and CI consume **committed** exported C only.

## Open the project

1. Install LVGL Pro Editor (Community).
2. Open the folder **`ui/`** (it is the Pro project: `project.xml`, `globals.xml`, `screens/`, `components/`).
3. Canvas target is **480×480**, LVGL **9.5**.

## Edit tokens and screens

1. **Tokens** live in `ui/globals.xml`. Replace Pro template defaults (purple accent, generic dark) with Figma dark tokens from `AGENTS.md` (`screen_bg` `#0d1329`, `card_bg` `#1a2651`, `accent` `#00d460`, `danger` `#fb2c36`, …). No raw style hex in `ui/logic/`.
2. Author shared components under `ui/components/` (StatusBar, ButtonBar, VitalCard, …).
3. Author the 8 triage screens under `ui/screens/`: Home → Scanning RFID → Scan Berhasil → Select Age → Select Gender → Mengukur → Scan Result → Monitor.
4. Validate layout in the Editor live preview (human). Copy is Bahasa Indonesia, match Figma exactly.

## Compile and export code (full project)

1. In the Editor: **Ctrl+B**, the hammer icon, or **Compile & export code**.
2. Export the **whole project**, not a single screen. Partial export is a known failure mode.
3. Commit the generated C after a clean export so sim/ESP builds work without the Editor installed.

### Required export artifacts checklist

A full export should leave (at least):

| Artifact | Why |
| --- | --- |
| `ui/ui.h` | Sim gates `HAS_UI` / `lib-ui` on this **and** `ui/CMakeLists.txt` |
| `ui/ui.c` | Project entry / glue |
| `ui/ui_gen.h`, `ui/ui_gen.c` | Generated project API |
| `ui/globals_gen.*` | When the Editor emits them (tokens/styles) |
| `ui/file_list_gen.cmake` | Source list for `lib-ui` |
| `ui/component_lib_list_gen.cmake` | Component list (if present) |
| Per-screen / per-component `*_gen.c` / `*_gen.h` | As listed in `file_list_gen.cmake` |

**Rules:**

- Never hand-edit `*_gen.c` / `*_gen.h`. Fix XML, re-export.
- `CMakeLists.txt` alone is not a complete export. Without `ui/ui.h` the sim builds a welcome label instead of the real UI (`HAS_UI` stays off).

## Run the SDL simulator

From the repo root:

```bash
cmake -B sim/build -S sim
cmake --build sim/build
./sim/build/bin/lvgl_simulator
```

- Window is **480×480**. Run from repo root (or use the `run` CMake target) so asset path `A:ui` resolves.
- With a complete export, `HAS_UI=1` and `lib-ui` link in. Without `ui/ui.h`, you only get the welcome fallback.
- Keypad path: mock buttons 0..3 map to `LV_KEY_PREV` / `NEXT` / `ENTER` / `ESC`. Touch ButtonBar cells and keys must both call **`ui_action(screen, btn_id)`** (single dispatcher). See `AGENTS.md` for the bar map.

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

1. Author XML in the Editor under `ui/screens/` (reuse StatusBar / ButtonBar / tokens).
2. Full-project **Compile & export code**; confirm new `*_gen.*` appear and `file_list_gen.cmake` lists them.
3. Commit generated C.
4. Wire logic: register show callback in `ui_nav`, add the per-screen action table in `ui_action`, fill session fields via `ui_runtime` if needed.
5. Extend a host selftest and/or walk the sim with keys.
6. Rebuild sim + `idf.py build`.

## Known blockers (honest status)

| Blocker | Detail |
| --- | --- |
| Incomplete / partial Pro export | Template demo export may exist (`ui.h`/`ui.c`/component gens) while triage screens and a full `globals_gen` set are still missing. Sim falls back without a complete pair of `CMakeLists.txt` + `ui.h`. |
| Figma tokens not yet in `globals.xml` | Pro defaults (e.g. purple accent) still present; Task 2 is human: edit tokens in the Editor, re-export. |
| Missing triage icons | Inventory has generic FontAwesome stand-ins; dedicated scan/RFID/SpO2/RR/BP/logo/… assets still need Figma export. |
| 8 triage screens not authored | Home … Monitor XML is human/Editor work. **Do not claim screens or UI are complete.** |
| Pro CLI | Not available. Export is GUI-only. |

Logic layer (`ui_types`, `ui_session`, `ui_mock`, `ui_input`, `ui_nav`, `ui_action`, `ui_runtime`) **is** done and host-selftestable. Screens, tokens, icons, and a complete export are the remaining human path.
