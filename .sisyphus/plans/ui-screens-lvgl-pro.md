# TriageBox Node UI — 8 Screens (LVGL Pro XML + C logic)

## TL;DR

> **Quick Summary**: Build all 8 TriageBox node screens as LVGL Pro XML (dark theme), export to C, and drive them with hand-written C navigation + mock data. No hardware integration — runs on laptop via SDL simulator and link-checks under ESP-IDF.
>
> **Deliverables**:
> - LVGL Pro project (`ui_pro/`) with dark design tokens + 3 shared components + 8 screens
> - Exported C under `ui/generated/` (never hand-edited)
> - Hand-written C: session model, navigation state machine, single `ui_action()` dispatch, keypad indev
> - Mock provider (vitals / buttons / RFID) with the exact struct shapes the STM32 serial task will later fill
> - `sim/` SDL2 target — runnable 480×480 preview on macOS
> - `main/` ESP-IDF target that compiles the same `ui/` (build-only, no panel bring-up)
> - AGENTS.md updated for the Pro workflow
>
> **Estimated Effort**: Large
> **Parallel Execution**: YES — 4 waves
> **Critical Path**: Task 1 → Task 2 → Task 3 → Task 8 → Task 20 → Task 24 → F1-F4 → user okay

---

## Execution Model & Tool Access (READ FIRST)

> The user has the **LVGL Pro Community (desktop Editor)** license. The **Pro CLI is a paid Professional feature and is NOT available.** This splits the work into a human lane and an agent lane. Every task below obeys this split.

**Human lane (user, in the desktop Editor — manual GUI):**
- Author/adjust component + screen XML with live preview and Figma Flow.
- Export C via the Editor's **"Compile & export code"** button into `ui/generated/`, then commit.
- This is the ONLY way generated C is produced. There is no headless `lved-cli.js generate/validate/screenshot` in this project.

**Agent lane (Xi Jinping — automated):**
- All hand-written C: `ui/logic/*` (types, session, mock, keypad, nav, action, runtime), `sim/`, `main/`.
- Build wiring for both targets, and ALL verification by **compiling the human-exported C** and **running the SDL sim binary** (screenshots come from the sim, not from the Pro CLI).
- The agent never needs to open the Editor or call the CLI.

**Global substitution (applies everywhere in this plan):**
- Wherever a task says "Pro CLI validate" → read as: *human validates visually in the Editor's live preview; agent validates by compiling the exported C.*
- Wherever a task says "Pro CLI generate/export" → read as: *human clicks Export in the Editor; agent consumes the committed `ui/generated/` output.*
- Wherever a task says "Pro CLI screenshot" → read as: *agent captures the SDL sim window.*

**Consequence:** Tasks that author XML (2, 9, 10, 11, 15–22) and the export steps in Tasks 8 and 23 are **human-owned GUI steps**. The agent's role for those is to (a) supply the exact per-screen spec/reference already written in each task, and (b) verify the exported result compiles and renders in the sim. Age/Gender, nav, mock, and all logic remain fully agent-owned. This is a deliberate human-in-the-loop at the export boundary — everything downstream stays agent-verified.

---

## Context

### Original Request
"bisa ga kamu bikin screen nya dulu, untuk integrasi nya nanti" — build the screens first, integration later. Plus: is hot reload possible, XML or what, can it preview on laptop.

### Interview Summary

**Key Discussions**:
- Hot reload: no true hot-reload in hand-written LVGL C. LVGL Pro's XML Editor has live pixel-perfect preview with no recompile — this is why the user chose it.
- XML vs C: user chose **LVGL Pro XML for visuals + hand-written C for logic** after being shown the trade-offs.
- Laptop preview: yes — Pro live preview during authoring, plus an SDL2 simulator so the exported C can be run without the editor.
- Scope: **all 8 screens in one plan**, **dark theme first**, **single repo with separated folders**.

**Research Findings**:
- LVGL Pro is free for personal/educational/open-source use (PKM-KC qualifies); v2.0 is product-licensed for commercial only.
- Pro exports `*_gen.c` / `*_gen.h` (overwritten every export) plus one-time user skeleton files. Custom code belongs only in the skeletons.
- Exported C is platform-independent and contains no drivers — we keep owning LVGL init and `esp_lvgl_port`.
- After export, `LV_USE_XML` can be `0`; XML is not needed at runtime.
- Figma dark tokens and all 8 dark screen node IDs verified via Figma MCP.

### Sun Tzu Review

**Identified Gaps** (addressed in this plan):
- Generated-C git policy undefined → **commit `*_gen.*` after export**; re-export then commit; never hand-edit.
- Font pipeline unspecified → **subset LVGL font C arrays** for the required sizes only; no runtime TTF.
- ESP-IDF depth ambiguous → **build/link only**; board bring-up (CH32, GT911, PSRAM fb) is explicitly out.
- Power / Menu behavior undefined → **no-op with log**; no Menu screen.
- 60-second measure window would stall QA → **accelerated mock timer** (~2 s) behind a compile-time define.
- Dual preview sequencing → Pro preview first, `sim/` only after the first export compiles.
- Pro subjects/data-binding vs plain C setters → **plain C setters**; XML stays dumb layout.
- Age/Gender skip paths → **no skip in v1**; linear flow only.

---

## Work Objectives

### Core Objective
Produce a complete, navigable 8-screen dark-theme TriageBox node UI that runs on a laptop and compiles for ESP32-S3, with all hardware inputs mocked behind interfaces that later serial/ML work can fill without touching UI code.

### Concrete Deliverables
- `ui_pro/project.xml` + token/style XML + `ui_pro/components/{status_bar,button_bar,vital_card}.xml`
- `ui_pro/screens/` — 8 screens: home, scanning, berhasil, age, gender, mengukur, result, monitor
- `ui/generated/*_gen.c|h` — exported, committed, never edited
- `ui/logic/ui_session.c|h`, `ui/logic/ui_nav.c|h`, `ui/logic/ui_action.c|h`, `ui/logic/ui_mock.c|h`
- `ui/logic/ui_types.h` — `vitals_t`, `btn_event_t`, `rfid_t` (stable for future serial)
- `sim/main.c`, `sim/CMakeLists.txt` — SDL2 480×480 runner
- `main/CMakeLists.txt`, `main/idf_component.yml`, `main/app_main.c` — ESP-IDF build target
- Updated `AGENTS.md`; `docs/ui-workflow.md`

### Definition of Done
- [ ] `cmake -S sim -B sim/build && cmake --build sim/build` exits 0
- [ ] `idf.py set-target esp32s3 && idf.py build` exits 0
- [ ] Sim binary boots to Home and survives 10 s without crash
- [ ] Documented key sequence walks all 8 screens and returns to Home
- [ ] `git grep -nE '#[0-9a-fA-F]{6}' ui/logic sim main` returns no style hex
- [ ] Zero LVGL v8 symbols: `git grep -c 'lv_indev_drv_t\|lv_disp_drv_t\|lv_disp_draw_buf_t'` = 0

### Must Have
- All 8 screens present, reachable, matching Figma dark layout (480×480; StatusBar y=0 h=48; ButtonBar y=409 h=71; 4×120×71 cells)
- Bahasa Indonesia copy exactly as in Figma
- One action table per screen; touch and keypad both call the same `ui_action()`
- Button indices 0–3 stable left→right; empty cells render but no-op
- Mock provider API identical in shape to future serial data
- Dark tokens centralized; no raw hex in logic

### Must NOT Have (Guardrails)
- Light theme, theme switcher, or light-variant XML
- STM32 serial/RS485 framing, any bytes on a wire
- LoRa TX, MQTT publish
- C5.0 inference or any "tiny if/else triage" — Result uses a hardcoded mock
- Real GPIO/expander button reads; invented free ESP32 GPIOs
- Waveshare board bring-up (CH32 init, GT911, PSRAM framebuffer) as a gate for "screens done"
- Menu screen, Settings screen, or any 9th screen
- Runtime XML loading (`LV_USE_XML=1`) — export-C path only
- Hand edits inside `*_gen.c` / `*_gen.h`
- Animations or transitions not present in Figma
- New dependencies beyond LVGL v9, SDL2 (sim only), ESP-IDF/BSP
- Pro subjects/data-binding graph for vitals in v1

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed.

### Test Decision
- **Infrastructure exists**: NO (empty repo)
- **Automated tests**: NO unit test framework — verification is build + scripted sim execution
- **Rationale**: this is a GUI shell with mocked I/O; build success plus scripted navigation through the sim binary is the meaningful signal. A unit framework would test generated code we do not own.

### QA Policy
Every task carries agent-executed QA scenarios. Evidence to `.sisyphus/evidence/task-{N}-{slug}.{ext}`.

- **XML/Pro tasks (human-exported)**: user exports from the Editor GUI; agent verifies by compiling the committed `ui/generated/` output and rendering it in the sim — exit codes, generated file listings, sim screenshots. **No Pro CLI** (Professional-only, not licensed).
- **Logic/C tasks**: compile via sim CMake, then run the sim binary with scripted key input under `timeout`
- **Sim runtime**: launch binary, send keystrokes, capture stdout/log + screenshot where available
- **ESP-IDF tasks**: `idf.py build` exit code + linker map presence

Mock data fixed across all QA: RFID `"3021"`, `hr=90 spo2=98 rr=18 bp_sys=120 bp_dia=80 battery=80`, demo priorities cycle `GREEN → YELLOW → RED → BLACK`.

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (foundation — must land before XML/screen bulk):
├── Task 1: LVGL Pro install + license + project skeleton [quick]
├── Task 2: Dark design tokens XML (globals/styles) [quick]
├── Task 3: ui_types.h — vitals_t/btn_event_t/rfid_t contracts [quick]
├── Task 4: Font subsetting (Inter sizes → LVGL C) [unspecified-high]
├── Task 5: Icon/asset extraction from Figma → LVGL images [visual-engineering]
├── Task 6: sim/ SDL2 skeleton (blank 480x480 window) [quick]
└── Task 7: main/ ESP-IDF skeleton + idf_component.yml (LVGL v9 pinned) [quick]

Wave 2 (shared components + export proof + logic core):
├── Task 8: Export pipeline proof — one screen XML → C → both targets compile (depends: 1,2,6,7) [deep]
├── Task 9: StatusBar XML component (depends: 2,4,5) [visual-engineering]
├── Task 10: ButtonBar XML component, 4 index-stable cells (depends: 2,4,5) [visual-engineering]
├── Task 11: VitalCard XML component (depends: 2,4,5) [visual-engineering]
├── Task 12: ui_session.c — session state (rfid/age/gender/vitals/priority) (depends: 3) [quick]
├── Task 13: ui_mock.c — mock vitals/RFID/button source + accelerated timer (depends: 3) [unspecified-high]
└── Task 14: keypad indev reading mock button buffer (depends: 3,6) [deep]

Wave 3 (the 8 screens — max parallel, all depend on Wave 2 components):
├── Task 15: Home screen XML (depends: 8,9,10) [visual-engineering]
├── Task 16: Scanning RFID screen XML (depends: 8,9,10) [visual-engineering]
├── Task 17: Scan Berhasil screen XML (depends: 8,9,10) [visual-engineering]
├── Task 18: Select Age screen XML (depends: 8,9,10) [visual-engineering]
├── Task 19: Select Gender screen XML (depends: 8,9,10) [visual-engineering]
├── Task 20: Mengukur screen XML (depends: 8,9,10,11) [visual-engineering]
├── Task 21: Scan Result screen XML (depends: 8,9,10,11) [visual-engineering]
└── Task 22: Monitor screen XML (depends: 8,9,10,11) [visual-engineering]

Wave 4 (wiring + docs):
├── Task 23: Export all screens to ui/generated + commit policy (depends: 15-22) [quick]
├── Task 24: ui_nav.c + ui_action.c — flow graph & per-screen action tables (depends: 12,14,23) [deep]
├── Task 25: Wire mock vitals into Mengukur/Result/Monitor live updates (depends: 13,24) [deep]
├── Task 26: sim/ full run — all screens navigable via keys (depends: 24,25) [unspecified-high]
├── Task 27: main/ app_main links full ui/ + idf build green (depends: 23,24) [unspecified-high]
└── Task 28: AGENTS.md update + docs/ui-workflow.md (depends: 23,26,27) [writing]

Wave FINAL (4 parallel reviews, then user okay):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high)
└── Task F4: Scope fidelity check (deep)
-> Present results -> Get explicit user okay

Critical Path: 1 → 2 → 8 → 10 → 20 → 23 → 24 → 26 → F1-F4 → okay
Parallel Speedup: ~65% vs sequential
Max Concurrent: 8 (Wave 3)
```

### Dependency Matrix

| Task | Depends On | Blocks |
| --- | --- | --- |
| 1 | — | 8, 9-11, 15-22 |
| 2 | — | 8, 9, 10, 11 |
| 3 | — | 12, 13, 14 |
| 4 | — | 9, 10, 11 |
| 5 | — | 9, 10, 11 |
| 6 | — | 8, 14 |
| 7 | — | 8 |
| 8 | 1, 2, 6, 7 | 15-22 |
| 9 | 2, 4, 5 | 15-22 |
| 10 | 2, 4, 5 | 15-22 |
| 11 | 2, 4, 5 | 20, 21, 22 |
| 12 | 3 | 24 |
| 13 | 3 | 25 |
| 14 | 3, 6 | 24 |
| 15-19 | 8, 9, 10 | 23 |
| 20-22 | 8, 9, 10, 11 | 23 |
| 23 | 15-22 | 24, 27, 28 |
| 24 | 12, 14, 23 | 25, 26, 27 |
| 25 | 13, 24 | 26 |
| 26 | 24, 25 | 28, F3 |
| 27 | 23, 24 | 28 |
| 28 | 23, 26, 27 | F1 |

### Agent Dispatch Summary

| Wave | Count | Assignment |
| --- | --- | --- |
| 1 | 7 | T1-T3 → `quick`, T4 → `unspecified-high`, T5 → `visual-engineering`, T6-T7 → `quick` |
| 2 | 7 | T8 → `deep`, T9-T11 → `visual-engineering`, T12 → `quick`, T13 → `unspecified-high`, T14 → `deep` |
| 3 | 8 | T15-T22 → `visual-engineering` |
| 4 | 6 | T23 → `quick`, T24-T25 → `deep`, T26-T27 → `unspecified-high`, T28 → `writing` |
| FINAL | 4 | F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep` |

---

## Key Decisions (do not re-litigate)

| Decision | Choice | Rationale |
| --- | --- | --- |
| Generated C in git | **Commit** `*_gen.*` | Keeps sim/ESP builds reproducible without Pro installed |
| Hand edits to gen files | **Forbidden** | Overwritten each export; fix XML instead |
| Font | Subset Inter (or metric substitute) → LVGL C, sizes 48/24/18/16/14/13/10/9 | Flash budget + no runtime TTF |
| Icons | Figma → PNG → `lv_image` C arrays | No SVG runtime in LVGL v9 embedded path |
| Vitals binding | Plain C setters | Keeps XML dumb; serial swap-in later is trivial |
| Screen lifetime | Permanent screens + `lv_screen_load` | Pro default; RAM measured in Task 26 |
| ESP target depth | **Build/link only** | Board bring-up is a separate effort |
| Power / Menu buttons | **No-op + log** | Undefined in Figma flow; prevents invented screens |
| Measure duration | Accelerated ~2 s via `UI_MEASURE_MS` define | Real 60 s blocks QA |
| Abort (Scanning/Mengukur) | → Home | Single documented target |
| Restart (Berhasil) | → Scanning, clear RFID | — |
| Reset (Result) | → Home, clear session | — |
| Stop (Monitor) | → Result | Chosen once, documented |
| Back (Age) | → Berhasil; Back (Gender) → Age | — |
| Age/Gender skip | Not in v1 | Linear flow only |
| Priority in v1 | Hardcoded mock + key to cycle all 4 | Avoids ML scope creep |
| Figma vs draft table conflict | **Figma wins** | Design is law per AGENTS.md |
| Export tool | **Desktop Editor "Compile & export code" button** (GUI, human-owned) | Pro CLI is Professional-only and unlicensed; agent never invokes it |
| Verification of XML | Live preview in Editor (human) + compile+sim render of exported C (agent) | Replaces all "Pro CLI validate/screenshot" references |

---

## TODOs

- [x] 1. LVGL Pro project skeleton (HUMAN-OWNED — desktop Editor)

  **Execution note**: User already has LVGL Pro Community installed. This task is done by the **user in the desktop Editor GUI**, not by the agent. The agent's role is to hand over this exact spec and, once the user confirms it's done, verify the resulting files on disk.

  **What to do (user, in Editor)**:
  - Open LVGL Pro Editor. Confirm the active license is **Community (non-commercial)** — PKM-KC educational use qualifies; no purchase needed.
  - Create a new project at `ui_pro/` named `triagebox_ui` (this name becomes the generated `triagebox_ui_init()` entry point — do not rename later, it cascades into every generated symbol).
  - Set the project/display canvas to **480×480**.
  - Create empty groups/folders for `components` and `screens` inside the project (`ui_pro/components/`, `ui_pro/screens/`).
  - Save the project so `ui_pro/project.xml` (or the Editor's equivalent project file) exists on disk.

  **What the agent does**:
  - Add `.gitignore` entries for Pro Editor local caches/build artifacts (NOT for generated C — that's committed in Task 23).
  - After the user confirms the project is saved, read `ui_pro/project.xml` and verify it declares 480×480 and the `triagebox_ui` project name.

  **Must NOT do**:
  - Agent must not attempt to invoke any `lved-cli.js` command — it is not licensed.
  - Do not author any screen or component XML yet (that starts at Task 2/9/10/11).
  - Do not enable runtime XML loading (`LV_USE_XML=1`).

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Agent side is just `.gitignore` + verifying a file the user created.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES (agent's `.gitignore` part); the human GUI step is a prerequisite gate before Wave 2 begins.
  - **Parallel Group**: Wave 1 (with Tasks 2-7)
  - **Blocks**: 8, 9, 10, 11, 15-22
  - **Blocked By**: None (but downstream XML tasks are blocked until the user confirms the project exists)

  **References**:

  **External References**:
  - <https://lvgl.io/docs/pro/editor/overview> — Editor concepts, project structure
  - <https://lvgl.io/docs/pro/editor/license> — confirms Community covers non-commercial/PKM use, and that the CLI is Professional-only
  - <https://lvgl.io/docs/pro/integration/using-exported-c-code> — explains that project name drives `<project>_init()`

  **WHY Each Reference Matters**:
  - The license page is what confirms the CLI is out of reach and the Editor GUI is the only export path — this shapes every downstream task's verification method.

  **Acceptance Criteria**:
  - [ ] `ui_pro/project.xml` exists on disk and declares a 480×480 target and project name `triagebox_ui`
  - [ ] `.gitignore` excludes Editor caches, not generated C

  **QA Scenarios**:

  ```
  Scenario: Project skeleton declares the correct canvas size and name
    Tool: Bash
    Preconditions: user has created and saved the project in the Editor
    Steps:
      1. `grep -n "480" ui_pro/project.xml` — assert both width and height 480 are present
      2. `grep -n "triagebox_ui" ui_pro/project.xml` — assert the project name is set
    Expected Result: 480×480 target and correct project name found
    Failure Indicators: missing file, different resolution, wrong/missing name
    Evidence: .sisyphus/evidence/task-1-project-xml.txt

  Scenario: No CLI dependency was introduced
    Tool: Bash
    Preconditions: task complete
    Steps:
      1. `grep -rn "lved-cli" . --include='*.sh' --include='*.yml' --include='CMakeLists.txt' --include='*.c' --include='*.h' | grep -v '^./.sisyphus/'`
      2. Assert zero matches (the plan/docs may *mention* the CLI as forbidden; build scripts and code must never invoke it)
    Expected Result: no CLI invocation in any script, build file, or source
    Failure Indicators: any lved-cli call wired into the build or a helper script
    Evidence: .sisyphus/evidence/task-1-no-cli.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-1-project-xml.txt`, `task-1-no-cli.txt`

  **Commit**: YES (groups with 2)
  - Message: `chore(ui): scaffold LVGL Pro project and dark tokens`
  - Files: `ui_pro/project.xml`, `.gitignore`
  - Pre-commit: agent confirms project.xml is correct

- [ ] 2. Dark design tokens as XML styles

  **What to do**:
  - Create `ui_pro/globals.xml` (or the Pro-idiomatic tokens file) defining the dark palette as named constants/styles:
    | Token | Value |
    | --- | --- |
    | `screen_bg` | `#0d1329` |
    | `card_bg` | `#1a2651` |
    | `border` | `#1a2651` |
    | `text_primary` | `#ffffff` |
    | `text_secondary` | `#99a1af` |
    | `text_on_card` | `#d1d5dc` |
    | `accent` | `#00d460` |
    | `status_ok` | `#00c950` |
    | `danger` | `#fb2c36` |
  - Define reusable styles: card (radius 10), pill (radius 100), status dot (12px round), ButtonBar cell (vertical gradient `#000827` → `#1a2651`, 1px border `#1a2651`).
  - Define text style roles matching Figma sizes: 48 bold, 24 bold, 18 bold, 18 semibold, 16 semibold, 14 regular, 13 regular, 10 regular, 9 regular.
  - Every color used later MUST reference a token here — no literals in component or screen XML.

  **Must NOT do**:
  - Do not add light-theme tokens or a second palette.
  - Do not define animation or transition styles.
  - Do not invent colors absent from Figma (notably YELLOW/GREEN/BLACK triage fills — only RED `#fb2c36` is confirmed).

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Transcribing a verified token table into XML; values already extracted.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3-7)
  - **Blocks**: 8, 9, 10, 11
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `AGENTS.md` → "UI contract (Figma is law)" token table — the authoritative dark values
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → "Design tokens" section, includes gradient and radius specifics

  **External References**:
  - <https://lvgl.io/docs/pro/editor/overview> — where styles/globals live in a Pro project

  **WHY Each Reference Matters**:
  - The skill file carries the ButtonBar gradient stops and radii that the AGENTS.md table omits; both are needed for a complete token set.

  **Execution note**: XML authoring happens in the **desktop Editor (user)**. The agent supplies this token table verbatim and verifies the saved XML on disk afterwards.

  **Acceptance Criteria**:
  - [ ] All 9 color tokens present with exact hex values
  - [ ] Card/pill/dot/bar-cell styles defined
  - [ ] Editor live preview shows no validation errors (user confirms); agent verifies token values on disk

  **QA Scenarios**:

  ```
  Scenario: Every dark token is present with the exact Figma value
    Tool: Bash
    Preconditions: tokens XML written
    Steps:
      1. For each of 0d1329, 1a2651, ffffff, 99a1af, d1d5dc, 00d460, 00c950, fb2c36 run grep -i against the tokens file
      2. Assert each returns at least one match
    Expected Result: 8 distinct hex values found (border reuses 1a2651)
    Failure Indicators: any missing hex, or a color not in the table appearing
    Evidence: .sisyphus/evidence/task-2-tokens-grep.txt

  Scenario: No light-theme token leaked in
    Tool: Bash
    Preconditions: tokens XML written
    Steps:
      1. `grep -iE 'fefefe|e5f1f9|34383f|16bc4e|e2e5e8' ui_pro/`
      2. Assert zero matches (these are light-theme values, deferred)
    Expected Result: no output — light palette absent
    Failure Indicators: any light hex present
    Evidence: .sisyphus/evidence/task-2-no-light.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-2-tokens-grep.txt`, `task-2-no-light.txt`

  **Commit**: YES (groups with 1)
  - Message: `chore(ui): scaffold LVGL Pro project and dark tokens`
  - Files: `ui_pro/globals.xml`
  - Pre-commit: token grep checks pass (agent, post-export)

- [x] 3. Mock data type contracts (`ui_types.h`)

  **What to do**:
  - Create `ui/logic/ui_types.h` defining the structs that mocks fill now and the STM32 serial task will fill later — the shape must not change when serial arrives:
    - `vitals_t`: `uint16_t hr, spo2, rr, bp_sys, bp_dia; uint8_t battery;` plus a validity/staleness flag.
    - `btn_event_t`: button index `0..3` + pressed/released state + timestamp ms.
    - `rfid_t`: fixed-capacity tag string buffer + present flag (must allow "no tag").
  - Define `ui_priority_t` enum with exactly `UI_PRIORITY_RED`, `UI_PRIORITY_YELLOW`, `UI_PRIORITY_GREEN`, `UI_PRIORITY_BLACK` — names matching the backend wire enum.
  - Define a display-label mapping helper declaration returning "MERAH - IMMEDIATE" / "KUNING - DELAYED" / "HIJAU - MINOR" / "HITAM - EXPECTANT".
  - Define age band enum (`6-17`, `18-45`, `46-60`, `>60`) and gender enum mapping to backend codes `M` / `F` / `U`.
  - Header must be pure C99, no LVGL includes, no platform ifdefs — it is shared by sim and firmware.

  **Must NOT do**:
  - Do not add serial framing fields (CRC, packet_version, sync bytes).
  - Do not add `priority`/`confidence`/`reasons` to `vitals_t` — those are produced by ML later, not received.
  - Do not include `lvgl.h` or any ESP-IDF header.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: A single header of plain data contracts.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1-2, 4-7)
  - **Blocks**: 12, 13, 14
  - **Blocked By**: None

  **References**:

  **API/Type References**:
  - `../triagebox-backend/src/common/mqtt-payload.ts:28-49` — canonical vital field names and types; mirror `hr/spo2/rr/bp_sys/bp_dia/battery` naming
  - `../triagebox-backend/src/common/mqtt-payload.ts:3` — `prioritySchema` enum: exactly RED/YELLOW/GREEN/BLACK
  - `AGENTS.md` → "Canonical vital JSON" and the triage label mapping table

  **WHY Each Reference Matters**:
  - Naming these structs after the backend keys now means the future serial task and LoRa packing need zero renaming — that is the whole point of freezing the contract early.
  - The label mapping must be exact Indonesian strings from Figma; getting it from AGENTS.md avoids invented copy.

  **Acceptance Criteria**:
  - [ ] `ui/logic/ui_types.h` compiles standalone: `cc -fsyntax-only -std=c99 ui/logic/ui_types.h`
  - [ ] Contains all 4 priority enum members
  - [ ] No `lvgl.h`, no `esp_` includes

  **QA Scenarios**:

  ```
  Scenario: Header compiles standalone as plain C99
    Tool: Bash
    Preconditions: ui/logic/ui_types.h written
    Steps:
      1. Run `cc -fsyntax-only -std=c99 -x c ui/logic/ui_types.h`
      2. Assert exit code 0 and no warnings about missing includes
    Expected Result: exit 0, no diagnostics
    Failure Indicators: missing type errors, include-not-found
    Evidence: .sisyphus/evidence/task-3-syntax.txt

  Scenario: Header is platform-neutral and ML-free
    Tool: Bash
    Preconditions: header written
    Steps:
      1. `grep -nE 'lvgl|esp_|confidence|reasons|crc|packet_version' ui/logic/ui_types.h`
      2. Assert zero matches
    Expected Result: no output
    Failure Indicators: any LVGL/ESP include, or ML/serial-only fields present
    Evidence: .sisyphus/evidence/task-3-neutral.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-3-syntax.txt`, `task-3-neutral.txt`

  **Commit**: YES
  - Message: `feat(ui): add mock data type contracts`
  - Files: `ui/logic/ui_types.h`
  - Pre-commit: `cc -fsyntax-only` exits 0

- [x] 4. Font subsetting — Inter sizes to LVGL C arrays

  **What to do**:
  - Determine the glyph set actually needed: ASCII letters/digits, Indonesian punctuation, `%`, `/`, `-`, `>`, `…`, and the degree-free unit strings used ("bpm", "mmHg", "/min").
  - Generate LVGL v9 font C arrays for the sizes Figma uses: **48, 24, 18, 16, 14, 13, 10, 9** (bold/semibold/regular weights only where a screen actually needs them — do not generate all weights at all sizes).
  - Use Inter if its license permits redistribution in firmware; otherwise substitute a metric-compatible open font and record the substitution.
  - Place under `ui/assets/fonts/`. Register them in the Pro project so XML can reference them by name.
  - Record the total `.rodata` cost of the fonts in evidence — this is the flash budget check.

  **Must NOT do**:
  - Do not enable runtime TTF/FreeType loading.
  - Do not generate full Unicode ranges or CJK.
  - Do not generate every weight×size combination "just in case".

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Requires license judgement, glyph-set analysis, and size/flash trade-off reasoning.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1-3, 5-7)
  - **Blocks**: 9, 10, 11
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → "Typography" line lists the exact sizes observed in Figma
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → "Copy" section — the actual strings that must render, which defines the glyph set

  **External References**:
  - LVGL v9 font converter documentation — for generating C arrays with the correct v9 struct
  - Inter font license terms — to decide redistribute vs substitute

  **WHY Each Reference Matters**:
  - The copy list is the ground truth for which glyphs are required; subsetting from it keeps flash small instead of guessing a range.

  **Acceptance Criteria**:
  - [ ] Font C files exist under `ui/assets/fonts/`
  - [ ] Each compiles: `cc -fsyntax-only` per file exits 0
  - [ ] Every Figma size has a corresponding font symbol
  - [ ] Total font size recorded in evidence

  **QA Scenarios**:

  ```
  Scenario: All required font sizes exist and compile
    Tool: Bash
    Preconditions: fonts generated under ui/assets/fonts/
    Steps:
      1. List ui/assets/fonts/*.c
      2. For sizes 48,24,18,16,14,13,10,9 assert a matching font symbol exists via grep
      3. Run `cc -fsyntax-only -I<lvgl> ` on each font .c
    Expected Result: every size present, all files compile exit 0
    Failure Indicators: a Figma size with no font, compile error, LVGL v8 font struct
    Evidence: .sisyphus/evidence/task-4-fonts.txt

  Scenario: Indonesian copy glyphs render (no missing glyph)
    Tool: Bash
    Preconditions: fonts generated
    Steps:
      1. Extract the unique character set from the copy list in the UI skill file
      2. Verify each character is within the generated font ranges (script or converter range report)
      3. Assert no character is unmapped
    Expected Result: zero unmapped characters
    Failure Indicators: any glyph in "Memindai RFID...", "MERAH - IMMEDIATE", ">60 tahun" missing
    Evidence: .sisyphus/evidence/task-4-glyph-coverage.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-4-fonts.txt`, `task-4-glyph-coverage.txt`, font byte sizes

  **Commit**: YES (groups with 5)
  - Message: `chore(ui): add subset fonts and icon assets`
  - Files: `ui/assets/fonts/*`
  - Pre-commit: per-file `cc -fsyntax-only` exits 0

- [ ] 5. Icon and asset extraction from Figma

  **What to do**:
  - Enumerate the icons used across the 8 dark screens: TriageBox logo mark, status-bar battery + link + clock glyphs, home status dots, Scan/Power/Menu/Start/Restart/Abort/Up/Down/Back/Select/Monitor/Reset/Stop button icons, RFID scan icon, result warning icon, vital icons (SpO2, HR, RR, BP).
  - Download each via the Figma MCP `download_assets` on the dark nodes (file `etAAzsnQu0RlnxnPYNBEJz`), preferring the node's export settings.
  - Convert to LVGL v9 `lv_image` C arrays (RGB565 or indexed as appropriate), placed under `ui/assets/icons/`.
  - Register them in the Pro project so XML references them by name.
  - Keep the ButtonBar icons monochrome-tintable so a single icon can render white or red (Power) via recolor rather than duplicate assets.

  **Must NOT do**:
  - Do not embed SVG for runtime (LVGL v9 embedded path uses raster/C arrays here).
  - Do not pull the light-theme icon variants.
  - Do not invent icons not present in Figma.

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Asset extraction + fidelity to the design; recolor strategy is a visual concern.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1-4, 6-7)
  - **Blocks**: 9, 10, 11
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → screen map + ButtonBar table — tells you which icon belongs to which cell
  - `AGENTS.md` → note that Power is always red `#fb2c36` (drives the tintable-icon decision)

  **External References**:
  - Figma file `etAAzsnQu0RlnxnPYNBEJz`, dark nodes Home `16:98` … Monitor `63:378`
  - Figma MCP `download_assets` — the extraction tool
  - LVGL v9 image converter docs — C array format

  **WHY Each Reference Matters**:
  - The ButtonBar table maps icon→cell so nothing is missed; the Power-red note prevents shipping two copies of every icon.

  **Acceptance Criteria**:
  - [ ] All icons referenced by the 8 screens exist under `ui/assets/icons/`
  - [ ] Each image C file compiles (`cc -fsyntax-only`)
  - [ ] Icons are recolorable (single channel/alpha) where used in the bar

  **QA Scenarios**:

  ```
  Scenario: Every button/vital icon has an asset
    Tool: Bash
    Preconditions: icons extracted
    Steps:
      1. Build the expected icon name list from the ButtonBar table (scan,power,menu,start,restart,abort,up,down,back,select,monitor,reset,stop) + vitals (spo2,hr,rr,bp) + logo + rfid + warning + battery + link
      2. For each, assert a file exists under ui/assets/icons/
    Expected Result: no missing icon
    Failure Indicators: any name without a file
    Evidence: .sisyphus/evidence/task-5-icon-inventory.txt

  Scenario: Icon C arrays compile
    Tool: Bash
    Preconditions: icons converted to C
    Steps:
      1. `cc -fsyntax-only -I<lvgl>` each ui/assets/icons/*.c
      2. Assert all exit 0
    Expected Result: all compile
    Failure Indicators: LVGL v8 image descriptor, syntax error
    Evidence: .sisyphus/evidence/task-5-icon-compile.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-5-icon-inventory.txt`, `task-5-icon-compile.txt`

  **Commit**: YES (groups with 4)
  - Message: `chore(ui): add subset fonts and icon assets`
  - Files: `ui/assets/icons/*`
  - Pre-commit: per-file `cc -fsyntax-only` exits 0

- [x] 6. SDL2 simulator skeleton (blank 480×480)

  **What to do**:
  - Install prerequisites on macOS: `brew install sdl2 cmake`.
  - Create `sim/main.c` that: `lv_init()`, creates a **480×480** window via `lv_sdl_window_create(480, 480)`, registers `lv_sdl_mouse_create()` (touch) and `lv_sdl_keyboard_create()`, then loops on `lv_timer_handler()` + a short `SDL_Delay`.
  - Create `sim/CMakeLists.txt` that fetches/links LVGL v9.5 (SDL enabled via `LV_USE_SDL`), links SDL2, and compiles the shared `ui/` sources (empty for now — just prove the include path and build wiring).
  - Add a minimal `lv_conf.h` for the sim: `LV_USE_SDL 1`, `LV_COLOR_DEPTH 16`, `LV_USE_OS LV_OS_NONE`, `LV_USE_CHART 1`, `LV_USE_THEME_DEFAULT 1`.
  - At this stage the window may show LVGL's default blank screen — that is the pass condition.

  **Must NOT do**:
  - Do not use SDL3 (incompatible with LVGL 9).
  - Do not use `lv_drivers` (LVGL 9 builds SDL internally).
  - Do not add screen or navigation code yet.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard LVGL SDL boilerplate from official docs.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1-5, 7)
  - **Blocks**: 8, 14
  - **Blocked By**: None

  **References**:

  **External References**:
  - <https://lvgl.io/docs/open/9.3/details/integration/ide/pc-simulator> — "Using SDL manually" minimal `main`
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → LVGL v9 notes (SDL2 only, SDL3 incompatible)

  **WHY Each Reference Matters**:
  - The official minimal main is the exact boilerplate; copying it avoids the classic SDL3/`lv_drivers` mistakes.

  **Acceptance Criteria**:
  - [ ] `cmake -S sim -B sim/build` configures (exit 0)
  - [ ] `cmake --build sim/build` compiles (exit 0)
  - [ ] `timeout 10 ./sim/build/triagebox_sim` runs without crash

  **QA Scenarios**:

  ```
  Scenario: Blank simulator builds and stays up
    Tool: Bash
    Preconditions: brew sdl2+cmake installed, sim/ written
    Steps:
      1. `cmake -S sim -B sim/build` — assert exit 0
      2. `cmake --build sim/build` — assert exit 0
      3. `timeout 10 ./sim/build/triagebox_sim; echo $?` — assert it ran ~10s (timeout kill 124 acceptable) not an early crash (139/134)
    Expected Result: builds; process survives, no segfault
    Failure Indicators: cmake error, link error, exit 139 (segv) / 134 (abort)
    Evidence: .sisyphus/evidence/task-6-sim-build.txt

  Scenario: SDL3 / lv_drivers not used
    Tool: Bash
    Preconditions: sim/ written
    Steps:
      1. `grep -rniE 'SDL3|lv_drivers' sim/`
      2. Assert zero matches
    Expected Result: no output
    Failure Indicators: any SDL3 or lv_drivers reference
    Evidence: .sisyphus/evidence/task-6-no-sdl3.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-6-sim-build.txt`, `task-6-no-sdl3.txt`

  **Commit**: YES (groups with 7)
  - Message: `chore(build): add SDL simulator and ESP-IDF targets`
  - Files: `sim/main.c`, `sim/CMakeLists.txt`, `sim/lv_conf.h`
  - Pre-commit: sim builds, runs 10s

- [x] 7. ESP-IDF target skeleton

  **What to do**:
  - Create the ESP-IDF project layout: top-level `CMakeLists.txt`, `main/CMakeLists.txt`, `main/idf_component.yml`, `main/app_main.c`.
  - Pin dependencies per AGENTS.md: `waveshare/esp32_s3_touch_lcd_4: "^3.0.0"`, `lvgl/lvgl: "^9.5.0"`, `espressif/esp_lvgl_port: "^2.8.0"`.
  - Add `sdkconfig.defaults` with the N16R8 shape: `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`, `CONFIG_ESPTOOLPY_FLASHMODE_QIO`, `CONFIG_SPIRAM_MODE_OCT`, `CONFIG_SPIRAM_SPEED_80M`, `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240`, `CONFIG_LV_USE_CHART`, `CONFIG_LV_USE_THEME_DEFAULT`.
  - `app_main.c` may be a stub that just calls `lvgl_port_init()` (or even less) — the goal is **`idf.py build` succeeds**, not a running panel.
  - Register the shared `ui/` directory as an IDF component/include path so later tasks link into it, but keep it empty-safe now.

  **Must NOT do**:
  - Do not implement CH32 expander, GT911, or RGB panel bring-up.
  - Do not require flashing or a physical board.
  - Do not add serial/LoRa/sensor components.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard ESP-IDF scaffold with a known dependency set.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1-6)
  - **Blocks**: 8
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `AGENTS.md` → "Build" section: exact `idf_component.yml` block and required sdkconfig symbols
  - `.agents/skills/waveshare-esp32-s3-touch-lcd-4/SKILL.md` → sdkconfig N16R8 + toolchain versions

  **WHY Each Reference Matters**:
  - AGENTS.md already froze the dependency versions and sdkconfig; copying avoids a silent LVGL v8 resolve.

  **Acceptance Criteria**:
  - [ ] `idf.py set-target esp32s3` succeeds
  - [ ] `idf.py build` exits 0
  - [ ] Dependency lock shows LVGL 9.x (not 8.x)

  **QA Scenarios**:

  ```
  Scenario: ESP-IDF skeleton builds for esp32s3
    Tool: Bash
    Preconditions: ESP-IDF ≥5.3 installed and exported
    Steps:
      1. `idf.py set-target esp32s3` — assert exit 0
      2. `idf.py build` — assert exit 0
      3. `grep -R "lvgl" managed_components/*/idf_component.yml || cat dependencies.lock | grep -A2 lvgl`
      4. Assert resolved LVGL version starts with 9
    Expected Result: build success, LVGL 9.x resolved
    Failure Indicators: build error, LVGL 8.x resolved, missing PSRAM config
    Evidence: .sisyphus/evidence/task-7-idf-build.txt

  Scenario: No board bring-up crept in
    Tool: Bash
    Preconditions: main/ written
    Steps:
      1. `grep -rniE 'st7701|gt911|ch32|rgb_panel|esp_lcd' main/`
      2. Assert zero matches
    Expected Result: no output — skeleton is build-only
    Failure Indicators: any panel/expander init present
    Evidence: .sisyphus/evidence/task-7-no-bringup.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-7-idf-build.txt`, `task-7-no-bringup.txt`

  **Commit**: YES (groups with 6)
  - Message: `chore(build): add SDL simulator and ESP-IDF targets`
  - Files: `CMakeLists.txt`, `main/*`, `sdkconfig.defaults`
  - Pre-commit: `idf.py build` exits 0

- [ ] 8. Export pipeline proof — one screen, both targets (HUMAN + AGENT)

  **Execution note**: This is the highest-risk unknown in the plan. Export is GUI-only (no CLI license) — the human does the export click, the agent proves the result compiles on both targets. Coordinate as a handoff: agent prepares the request, user performs the Editor step, agent verifies.

  **What to do (user, in Editor)**:
  - Author ONE throwaway/minimal screen (a plain rectangle + label using tokens from Task 2) — not a real TriageBox screen, just a proof vehicle.
  - Click **"Compile & export code"** in the Editor. Confirm `*_gen.c` / `*_gen.h` are written into `ui/generated/` (point the export path there, or move the output there if the Editor exports elsewhere by default).

  **What the agent does**:
  - Wire `ui/generated/` into `sim/CMakeLists.txt`; confirm the sim target compiles and can `lv_screen_load()` the exported proof screen.
  - Wire the same `ui/generated/` into the ESP-IDF `main/` component; confirm `idf.py build` still succeeds with the generated files included.
  - Write `docs/ui-workflow.md` (create now, expand in Task 28) documenting: the exact Editor menu path for export, the target output folder, and the "never hand-edit `*_gen.*`" rule.
  - After the proof succeeds on both targets, either delete the throwaway screen's XML or keep it as a `smoke_test` screen excluded from the 8 real screens — record which was chosen.

  **Must NOT do**:
  - Agent must not attempt any `lved-cli.js` invocation.
  - Do not hand-edit anything under `ui/generated/`.
  - Do not enable `LV_USE_XML=1` — export-C only.
  - Do not proceed to Wave 2/3 component or screen work before this proof passes both builds.

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Even with the human doing the click, wiring two build systems to consume Editor-exported C correctly (and diagnosing any mismatch) is the plan's riskiest technical step.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: NO — gating task
  - **Parallel Group**: Sequential (Wave 2 start)
  - **Blocks**: 15, 16, 17, 18, 19, 20, 21, 22
  - **Blocked By**: 1, 2, 6, 7

  **References**:

  **External References**:
  - <https://lvgl.io/docs/pro/integration/using-exported-c-code> — exact export→integrate steps, `_gen` file naming, `<project>_init()` contract, "never edit generated files" rule, and the manual export-button workflow in the Editor (not the CLI)

  **WHY Each Reference Matters**:
  - This page is the single source of truth for how exported code must be wired into an app (`lv_init()`, create display/indev, call `<project>_init("")`, then `lv_screen_load(...)`) — skipping it risks guessing an incompatible integration.

  **Acceptance Criteria**:
  - [ ] `ui/generated/*_gen.c` and `*_gen.h` exist after the user's Editor export
  - [ ] `sim` target compiles and runs, loading the proof screen
  - [ ] `idf.py build` succeeds with `ui/generated/` included
  - [ ] `docs/ui-workflow.md` documents the exact Editor export steps (menu path, not CLI)

  **QA Scenarios**:

  ```
  Scenario: Human-exported C is consumable by the sim target
    Tool: Bash
    Preconditions: user has clicked export in the Editor; ui/generated/*_gen.{c,h} exist
    Steps:
      1. `ls ui/generated/*_gen.c ui/generated/*_gen.h` — assert files exist
      2. `cmake --build sim/build` — assert exit 0
      3. `timeout 5 ./sim/build/triagebox_sim` and confirm via log/screenshot that the proof screen rendered (not LVGL's blank default)
    Expected Result: sim shows the proof screen
    Failure Indicators: missing gen files, sim link error, blank/default screen only
    Evidence: .sisyphus/evidence/task-8-export-sim.txt, task-8-sim-screenshot.png

  Scenario: Same generated C compiles for ESP-IDF
    Tool: Bash
    Preconditions: ui/generated/ populated, main/ wired
    Steps:
      1. `idf.py build` — assert exit 0
      2. Confirm the generated sources appear in the build (component listing or object files)
    Expected Result: idf build succeeds including generated sources
    Failure Indicators: build error referencing ui/generated files, missing symbol
    Evidence: .sisyphus/evidence/task-8-export-idf.txt

  Scenario: No CLI dependency was introduced to achieve this
    Tool: Bash
    Preconditions: task complete
    Steps:
      1. `grep -rn "lved-cli" . --include='*.sh' --include='*.yml' --include='CMakeLists.txt' --include='*.c' --include='*.h' | grep -v '^./.sisyphus/'` — assert zero matches
    Expected Result: proof achieved entirely via Editor GUI export + agent build wiring; no CLI wired into any build/script
    Failure Indicators: any CLI invocation in build files or scripts
    Evidence: .sisyphus/evidence/task-8-no-cli.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-8-export-sim.txt`, `task-8-sim-screenshot.png`, `task-8-export-idf.txt`, `task-8-no-cli.txt`

  **Commit**: YES
  - Message: `feat(ui): prove XML to C export across both targets`
  - Files: `ui/generated/*` (proof only, may be superseded in Task 23), `docs/ui-workflow.md`
  - Pre-commit: sim + idf both build

- [ ] 9. StatusBar XML component

  **What to do**:
  - Author `ui_pro/components/status_bar.xml`: 480×48, left group at x=20 (battery icon 24px + percentage label, then link icon + status label), clock right-aligned at x=397. Match `36:1606` / other StatusBar node layout exactly (spacing, icon size, text style).
  - Expose component properties for: battery percent (int), link label (string, e.g. "Connected"), clock label (string, e.g. "09:14") — so screens/logic can set these without touching XML per-screen.
  - Use only tokens from Task 2 and icons from Task 5.
  - Validate visually against the Figma screenshot for Home (`16:98`) — the status bar region must match pixel-for-pixel in spacing/sizing.

  **Must NOT do**:
  - Do not hardcode "80%" / "Connected" / "09:14" as static text — these must be settable properties.
  - Do not add real battery/RTC/LoRa logic here — this is presentation only.

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Pixel-accurate layout work directly from Figma reference.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 10, 11, 12, 13, 14)
  - **Blocks**: 15-22
  - **Blocked By**: 2, 4, 5

  **References**:

  **Pattern References**:
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → "Layout skeleton" StatusBar spec
  - Figma dark Home screenshot captured earlier in this session (battery/link/clock layout)

  **External References**:
  - Figma node `16:98` (or any screen's `Frame 16` sub-node) via `get_design_context` for exact StatusBar spacing

  **WHY Each Reference Matters**:
  - Every screen reuses this exact component — getting spacing right once here avoids 8x rework.

  **Execution note**: Authored by the **user in the desktop Editor** using this spec; validated in live preview. Agent verifies the saved XML and the exported result once available.

  **Acceptance Criteria**:
  - [ ] Component exposes battery/link/clock as settable properties, no hardcoded demo values
  - [ ] Editor live preview shows no errors (user confirms)
  - [ ] Visual match against Figma Home status bar region (user confirms in preview; agent confirms once exported + rendered in sim)

  **QA Scenarios**:

  ```
  Scenario: StatusBar exposes settable properties, not static text
    Tool: Bash
    Preconditions: status_bar.xml saved by the Editor
    Steps:
      1. `grep -n "80%\|Connected\|09:14" ui_pro/components/status_bar.xml`
      2. Assert zero literal matches (values should be bound to properties/placeholders, not hardcoded)
    Expected Result: no hardcoded demo strings
    Failure Indicators: literal "80%" etc. present as static text
    Evidence: .sisyphus/evidence/task-9-no-hardcode.txt

  Scenario: Component renders correctly once exported (agent, after Task 23 export)
    Tool: Bash
    Preconditions: component exported into ui/generated and placed on a test screen
    Steps:
      1. Build sim, load a screen embedding the StatusBar, screenshot
      2. Assert status bar occupies y=0..48 with battery/link/clock laid out per Figma
    Expected Result: StatusBar renders at the right place and size
    Failure Indicators: missing/offset bar, wrong spacing
    Evidence: .sisyphus/evidence/task-9-render.png
  ```

  **Evidence to Capture**:
  - [ ] `task-9-no-hardcode.txt`, `task-9-render.png`

  **Commit**: YES (groups with 10, 11)
  - Message: `feat(ui): add StatusBar, ButtonBar, VitalCard components`
  - Files: `ui_pro/components/status_bar.xml`
  - Pre-commit: no-hardcode grep passes

- [ ] 10. ButtonBar XML component (4 index-stable cells)

  **What to do**:
  - Author `ui_pro/components/button_bar.xml`: 480×71 at y=409, 4 cells of exactly 120×71, each with icon (24×24 at y=12) + label (16px semibold at y=40), using the gradient cell background token from Task 2.
  - Expose 4 slots as component properties: per-cell icon, per-cell label, per-cell enabled flag. An empty slot renders the bar-cell background with no icon/label (matches Home cell 0, Scanning cell 1).
  - The "Power" label cell must support a red text variant (token `danger`) distinct from the other three (white).
  - Cell indices are fixed 0→3 left-to-right and must never reorder — this is the contract the keypad (Task 14) and action tables (Task 24) rely on.

  **Must NOT do**:
  - Do not bind cell actions here — ButtonBar is presentation only; navigation wiring happens in Task 24.
  - Do not make cell count configurable — always exactly 4.

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Pixel-accurate, reused-everywhere component.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 9, 11, 12, 13, 14)
  - **Blocks**: 15-22
  - **Blocked By**: 2, 4, 5

  **References**:

  **Pattern References**:
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → ButtonBar cell spec + full per-screen label table (this is the master list of every label/icon needed)
  - `AGENTS.md` → "Power" is always red across both themes

  **WHY Each Reference Matters**:
  - The per-screen label table in the skill file is exhaustive — use it to confirm every icon from Task 5 is actually consumed here.

  **Execution note**: Authored by the **user in the desktop Editor**; validated in live preview. Agent verifies via grep (on saved XML) and via sim render once exported.

  **Acceptance Criteria**:
  - [ ] Exactly 4 fixed-index slots, each independently settable (icon/label/enabled)
  - [ ] Power label supports red variant
  - [ ] Editor live preview shows no errors (user confirms)

  **QA Scenarios**:

  ```
  Scenario: Component exposes exactly 4 independent, index-stable slots
    Tool: Bash
    Preconditions: button_bar.xml saved by the Editor
    Steps:
      1. Grep the saved XML for 4 distinct slot property groups (icon+label+enabled per slot)
      2. Assert exactly 4 sets, indexed 0-3
    Expected Result: 4 independent slots found
    Failure Indicators: fewer/more than 4, shared/coupled slot properties
    Evidence: .sisyphus/evidence/task-10-slots.txt

  Scenario: Empty cell renders without icon/label (agent, after export)
    Tool: Bash
    Preconditions: component exported, test screen sets slot 0 empty and slots 1-3 with sample icon/label
    Steps:
      1. Build and run sim, screenshot the test screen
      2. Assert slot 0 shows only the cell background, no broken icon/label
    Expected Result: empty cell shows background only
    Failure Indicators: placeholder text, missing-icon glyph, layout shift
    Evidence: .sisyphus/evidence/task-10-empty-cell.png
  ```

  **Evidence to Capture**:
  - [ ] `task-10-slots.txt`, `task-10-empty-cell.png`

  **Commit**: YES (groups with 9, 11)
  - Message: `feat(ui): add StatusBar, ButtonBar, VitalCard components`
  - Files: `ui_pro/components/button_bar.xml`
  - Pre-commit: slot-count grep passes

- [ ] 11. VitalCard XML component

  **What to do**:
  - Author `ui_pro/components/vital_card.xml` covering both card styles seen in Figma: (a) the compact 4-up card row on Scan Result (icon + big number + small unit label, `card_bg`, radius 10), and (b) the larger Mengukur/Monitor vital block (icon + label + number + unit, larger type).
  - Expose properties: icon, value (string/number), unit label, and a size/variant switch if one XML component covers both, OR create two components (`vital_card_compact.xml`, `vital_card_large.xml`) if that is visually cleaner — pick one approach and document why.
  - Values must be settable properties (no hardcoded "92", "106", "18" demo numbers).

  **Must NOT do**:
  - Do not hardcode demo vital numbers.
  - Do not add live-update/timer logic here — pure presentation.

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Layout fidelity across two card variants.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 9, 10, 12, 13, 14)
  - **Blocks**: 20, 21, 22
  - **Blocked By**: 2, 4, 5

  **References**:

  **Pattern References**:
  - Figma dark Result (`16:1008`) and Monitor (`63:378`) design context captured earlier this session — exact card dimensions, spacing, and font sizes for both variants

  **WHY Each Reference Matters**:
  - Both card variants were already pulled via `get_design_context` in this session; reuse those exact px/spacing values instead of re-deriving them.

  **Execution note**: Authored by the **user in the desktop Editor**; validated in live preview. Agent verifies via grep and via sim render once exported.

  **Acceptance Criteria**:
  - [ ] All numeric/label values are settable properties, none hardcoded
  - [ ] Both compact and large variants covered
  - [ ] Editor live preview shows no errors (user confirms)

  **QA Scenarios**:

  ```
  Scenario: No hardcoded demo vitals in the component
    Tool: Bash
    Preconditions: vital_card XML saved by the Editor
    Steps:
      1. `grep -nE '"(92|106|18|93|78|76|17|14)"' ui_pro/components/vital_card*.xml`
      2. Assert zero matches
    Expected Result: no literal demo numbers
    Failure Indicators: any hardcoded vital value found
    Evidence: .sisyphus/evidence/task-11-no-hardcode.txt

  Scenario: Both variants render correctly once exported (agent, after Task 23)
    Tool: Bash
    Preconditions: components exported, placed on a test screen with sample properties set
    Steps:
      1. Build sim, load the test screen, screenshot both the compact and large variant
      2. Assert both match the recorded Figma dimensions/spacing
    Expected Result: both card variants render correctly
    Failure Indicators: missing variant, wrong size/spacing
    Evidence: .sisyphus/evidence/task-11-render.png
  ```

  **Evidence to Capture**:
  - [ ] `task-11-no-hardcode.txt`, `task-11-render.png`

  **Commit**: YES (groups with 9, 10)
  - Message: `feat(ui): add StatusBar, ButtonBar, VitalCard components`
  - Files: `ui_pro/components/vital_card*.xml`
  - Pre-commit: no-hardcode grep passes

- [x] 12. `ui_session.c` — session state

  **What to do**:
  - Create `ui/logic/ui_session.h/.c` holding the current triage session: RFID tag (or absent), selected age band, selected gender, latest `vitals_t`, current `ui_priority_t` (+ confidence/reasons as strings, populated by mock/later-ML), measurement progress.
  - Provide `ui_session_reset()` (clears everything, used on Home/Reset), `ui_session_new_scan(rfid)`, `ui_session_set_age(...)`, `ui_session_set_gender(...)`, `ui_session_set_vitals(vitals_t)`, `ui_session_set_priority(...)`.
  - Session is a single global instance (this is a single-node device, one active patient at a time) — do not build multi-session/multi-instance support.

  **Must NOT do**:
  - Do not include any LVGL widget code here — pure data/state.
  - Do not add persistence (no flash/NVS save) — session is RAM-only and resets on Home/Reset/reboot.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Straightforward state container with clear setter/getter API.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 9, 10, 11, 13, 14)
  - **Blocks**: 24
  - **Blocked By**: 3

  **References**:

  **Pattern References**:
  - `ui/logic/ui_types.h` (Task 3) — the structs this session stores

  **WHY Each Reference Matters**:
  - Session must use the exact same types the mock provider and later serial code use — no parallel/duplicate type definitions.

  **Acceptance Criteria**:
  - [ ] Compiles standalone against `ui_types.h`
  - [ ] `ui_session_reset()` clears all fields to defined defaults (not garbage)
  - [ ] No LVGL includes

  **QA Scenarios**:

  ```
  Scenario: Session reset clears all fields deterministically
    Tool: Bash
    Preconditions: ui_session.c compiled into a tiny test harness (or sim, once available)
    Steps:
      1. Write a throwaway harness: set_vitals(sample), set_priority(RED), then ui_session_reset()
      2. Assert rfid is absent, priority is a defined "none/unset" state, vitals are zeroed
    Expected Result: reset returns session to a clean documented default
    Failure Indicators: stale values survive reset, undefined/garbage state
    Evidence: .sisyphus/evidence/task-12-reset.txt

  Scenario: No LVGL dependency leaked into session logic
    Tool: Bash
    Preconditions: ui_session.c/.h written
    Steps:
      1. `grep -n "lvgl\|lv_" ui/logic/ui_session.*`
      2. Assert zero matches
    Expected Result: no LVGL references
    Failure Indicators: any lv_ symbol used
    Evidence: .sisyphus/evidence/task-12-no-lvgl.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-12-reset.txt`, `task-12-no-lvgl.txt`

  **Commit**: YES (groups with 13, 14)
  - Message: `feat(ui): add session model, mock provider, keypad indev`
  - Files: `ui/logic/ui_session.{h,c}`
  - Pre-commit: standalone compile passes

- [x] 13. `ui_mock.c` — mock vitals/RFID/button source + accelerated timer

  **What to do**:
  - Create `ui/logic/ui_mock.h/.c` providing: a fixed mock RFID `"3021"` returned after a short simulated "scan" delay; a mock `vitals_t` generator producing `hr=90, spo2=98, rr=18, bp_sys=120, bp_dia=80, battery=80` with small jitter for the Monitor screen's "live" feel; a mock priority cycle (`GREEN → YELLOW → RED → BLACK`) advanced by a debug key so QA can see all 4 Result variants.
  - Implement the measurement duration behind `#define UI_MEASURE_MS` (default an accelerated value, e.g. 2000, NOT 60000) so QA doesn't wait a real minute — document this clearly as a mock-only shortcut, not the real 1-minute spec.
  - Expose a poll-style API (`ui_mock_tick(uint32_t now_ms)`) that the LVGL timer calls — no threads, no blocking sleeps.

  **Must NOT do**:
  - Do not implement any serial/UART reading — this is 100% synthetic data.
  - Do not default `UI_MEASURE_MS` to the real 60000 ms (that belongs to the real hardware phase, not this mock).
  - Do not produce `confidence`/`reasons` beyond a hardcoded mock string — no rule evaluation logic (that would be scope creep into ML).

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Needs to model timing/jitter/cycling correctly without introducing hidden threading bugs.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 9, 10, 11, 12, 14)
  - **Blocks**: 25
  - **Blocked By**: 3

  **References**:

  **Pattern References**:
  - `../triagebox-backend/simulator/index.js:112-157` (`vitalsFor`) — realistic vital ranges per priority, useful reference for believable mock jitter ranges even though this mock is simpler
  - `ui/logic/ui_types.h` (Task 3)

  **WHY Each Reference Matters**:
  - The backend simulator's per-priority vital ranges are a ready-made reference for what "looks realistic" per priority without inventing numbers from scratch.

  **Acceptance Criteria**:
  - [ ] `ui_mock_tick()` is non-blocking (no `sleep`/`SDL_Delay` inside it)
  - [ ] `UI_MEASURE_MS` defaults to an accelerated value, documented in a comment
  - [ ] Priority cycle covers exactly the 4 enum values

  **QA Scenarios**:

  ```
  Scenario: Mock tick is non-blocking
    Tool: Bash
    Preconditions: ui_mock.c written
    Steps:
      1. `grep -nE 'sleep|SDL_Delay|vTaskDelay' ui/logic/ui_mock.c`
      2. Assert zero matches inside ui_mock_tick (blocking must live in the caller's loop, not here)
    Expected Result: no blocking calls in mock logic
    Failure Indicators: any sleep/delay call found
    Evidence: .sisyphus/evidence/task-13-nonblocking.txt

  Scenario: Measurement duration is accelerated, not the real 60s
    Tool: Bash
    Preconditions: ui_mock.h defines UI_MEASURE_MS
    Steps:
      1. `grep -n "UI_MEASURE_MS" ui/logic/ui_mock.h`
      2. Assert the defined value is <= 5000 (ms) with a comment noting it's a QA shortcut vs the real 60000ms spec
    Expected Result: accelerated default confirmed
    Failure Indicators: value is 60000 or undocumented
    Evidence: .sisyphus/evidence/task-13-measure-ms.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-13-nonblocking.txt`, `task-13-measure-ms.txt`

  **Commit**: YES (groups with 12, 14)
  - Message: `feat(ui): add session model, mock provider, keypad indev`
  - Files: `ui/logic/ui_mock.{h,c}`
  - Pre-commit: standalone compile passes

- [x] 14. Keypad indev reading a mock button buffer

  **What to do**:
  - Create `ui/logic/ui_input.h/.c` implementing an `LV_INDEV_TYPE_KEYPAD` indev per the pattern in the UI skill file: a `read_cb` that reads the **latest button state from a shared buffer** (not `gpio_get_level()`), maps to `LV_KEY_PREV/NEXT/ENTER/ESC`, and keeps the last key while reporting RELEASED.
  - In the sim, the shared buffer is filled by SDL keyboard events (map 4 keys, e.g. `1/2/3/4` or `a/s/d/f`, to button indices 0-3 — document the exact mapping in `docs/ui-workflow.md`).
  - Create the `lv_group_t` plumbing helper (`ui_input_create_group()`) for list screens (Age/Gender) per the skill file's guidance — `lv_group_create`, `lv_group_add_obj`, `lv_indev_set_group`.
  - Do NOT resolve the ButtonBar-vs-focus question generically here — Task 14 only provides the indev + group primitives; Task 24 decides which screens use focus vs direct action-table dispatch.

  **Must NOT do**:
  - Do not call `gpio_get_level()` anywhere.
  - Do not call LVGL functions from an interrupt/ISR context (moot in sim, but keep the pattern ISR-safe for later firmware reuse).
  - Do not hardcode navigation actions inside the indev callback — it only emits `LV_KEY_*`, nothing screen-specific.

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Correctness-sensitive input plumbing that both sim and firmware will rely on unchanged.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 9, 10, 11, 12, 13)
  - **Blocks**: 24
  - **Blocked By**: 3, 6

  **References**:

  **Pattern References**:
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → "Input: touch + 4 physical buttons" — the exact `read_cb` contract and key mapping this task must implement
  - LVGL v9 research captured this session: complete minimal keypad `read_cb` example with active-low debounced GPIO pattern (adapt the "read from buffer" version, not the raw GPIO version, since buttons are on STM32/mock)

  **WHY Each Reference Matters**:
  - The skill file's key mapping (`PREV/NEXT/ENTER/ESC`) and "read from shared buffer, never gpio_get_level" rule are the exact constraints this task must satisfy — deviating breaks the future STM32 integration contract.

  **Acceptance Criteria**:
  - [ ] `read_cb` never calls `gpio_get_level`
  - [ ] Last-key-while-released behavior implemented
  - [ ] `ui_input_create_group()` exists and is usable by list screens

  **QA Scenarios**:

  ```
  Scenario: Keypad indev reports correct LV_KEY_* for each mock button
    Tool: Bash
    Preconditions: sim built with ui_input.c linked, throwaway test screen with a focusable label group
    Steps:
      1. Launch sim, send SDL keydown for the mapped PREV key
      2. Assert (via debug log printed on LV_EVENT_KEY) that LV_KEY_PREV was received
      3. Repeat for NEXT/ENTER/ESC mapped keys
    Expected Result: each of the 4 keys produces exactly one corresponding LV_KEY_* event
    Failure Indicators: wrong key mapped, event missing, double-fire
    Evidence: .sisyphus/evidence/task-14-keymap.txt

  Scenario: No GPIO calls present in the indev implementation
    Tool: Bash
    Preconditions: ui_input.c written
    Steps:
      1. `grep -n "gpio_get_level" ui/logic/ui_input.c`
      2. Assert zero matches
    Expected Result: no direct GPIO reads
    Failure Indicators: any gpio_get_level call
    Evidence: .sisyphus/evidence/task-14-no-gpio.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-14-keymap.txt`, `task-14-no-gpio.txt`

  **Commit**: YES (groups with 12, 13)
  - Message: `feat(ui): add session model, mock provider, keypad indev`
  - Files: `ui/logic/ui_input.{h,c}`
  - Pre-commit: standalone compile passes

### Wave 3 — the 8 screens (shared conventions)

> All of Tasks 15-22 follow these identical rules. Read once, apply to every screen task.
>
> - File: `ui_pro/screens/<name>.xml`. Canvas **480×480**, `screen_bg` token background.
> - Compose `status_bar` (Task 9) at y=0 and `button_bar` (Task 10) at y=409 — never re-draw them inline.
> - Content lives between y=48 and y=409.
> - Colors/fonts/icons **only** via tokens (Task 2) and assets (Tasks 4, 5). Zero raw hex.
> - Indonesian copy **verbatim** from Figma — copy from `.agents/skills/lvgl-triagebox-ui/SKILL.md` "Copy" section, do not retype from memory.
> - Set the ButtonBar's 4 slots per that screen's row in the skill file's ButtonBar table. Empty slot = empty, not omitted.
> - Dynamic values (RFID, vitals, progress, priority) are **settable properties/placeholders**, never hardcoded demo text.
> - **Must NOT**: wire navigation/actions (Task 24 does that), add animations, author light-theme variants, or create a 9th screen.
> - **Agent**: `visual-engineering` — every one of these is pixel-fidelity layout work.
> - **Parallel**: all 8 run concurrently in Wave 3. Blocked by 8, 9, 10 (and 11 for screens with vital cards). Each blocks 23.
> - **Ownership**: XML authoring is **user-owned in the desktop Editor** (live preview validates as you go). The agent's job per screen is to (a) hand over the spec below verbatim, (b) after Task 23's export, verify the screen compiles and renders correctly in the sim.
> - **Per-screen QA (applies to all)**:
>
>   ```
>   Scenario: Screen matches Figma layout skeleton (agent, after export)
>     Tool: Bash
>     Preconditions: screen exported into ui/generated, sim builds
>     Steps:
>       1. Build sim, load this screen, capture a screenshot of the SDL window
>       2. Compare against the Figma dark node screenshot: assert status bar occupies y=0..48, button bar y=409..480, 4 equal 120px cells
>     Expected Result: layout regions and cell widths match Figma
>     Failure Indicators: shifted bars, wrong cell widths, missing component
>     Evidence: .sisyphus/evidence/task-{N}-{screen}-render.png
>
>   Scenario: No raw hex and no hardcoded dynamic values
>     Tool: Bash
>     Preconditions: screen XML saved by the Editor
>     Steps:
>       1. `grep -nE '#[0-9a-fA-F]{6}' ui_pro/screens/<name>.xml` — assert zero matches
>       2. `grep -nE '"(3021|P-2026-259|92|106|80%)"' ui_pro/screens/<name>.xml` — assert zero matches
>     Expected Result: tokens only; dynamic text bound to properties
>     Failure Indicators: any literal hex or demo value
>     Evidence: .sisyphus/evidence/task-{N}-{screen}-tokens.txt
>   ```
>
> - **Commit (each)**: `feat(ui): add {screen} screen`, files `ui_pro/screens/<name>.xml`, pre-commit = token/hardcode grep passes.

- [ ] 15. Home screen XML (`16:98`)

  **What to do**:
  - Author `ui_pro/screens/home.xml` per the shared Wave 3 conventions.
  - Content: 48px logo icon + "TriageBox" (48px bold, `text_primary`); "Sistem Triase Cerdas" (18px semibold, `accent`); "Siap untuk memulai triase pasien bencana" (14px regular, `text_secondary`); a `card_bg` info panel (radius 10) reading "Tekan **START** untuk memindai gelang RFID pasien" where only "START" is `accent` semibold and the rest is `text_on_card`; a 3-up status row of 12px round `status_ok` dots with 10px labels "Sistem OK", "Sensor OK", "LoRa OK".
  - ButtonBar slots: `[empty] / Scan / Power / Menu`.

  **References**:
  - Figma dark node `16:98` — full design context already captured this session (exact positions: HomeScreen at x=66 y=64, logo container, info panel at y=178, status row at y=280.5)
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → Copy section, Home strings

  **Blocked By**: 8, 9, 10 · **Blocks**: 23

- [ ] 16. Scanning RFID screen XML (`16:302`)

  **What to do**:
  - Author `ui_pro/screens/scanning.xml`.
  - Content: large (~85px) RFID scan icon centered; "Memindai RFID..." (36px line-height heading style); "Dekatkan gelang pasien ke sensor" (14px `text_secondary`); a 3-dot progress indicator (three 12px dots, 20px apart) that will later animate — render static here.
  - ButtonBar slots: `Abort / [empty] / Power / Menu`.

  **References**:
  - Figma dark node `16:302` — ScanScreen at x=130.49 y=88.63, icon 84.76px, text block at y=112, dots at y=208.37
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → Copy section, Scanning strings

  **Blocked By**: 8, 9, 10 · **Blocks**: 23

- [ ] 17. Scan Berhasil screen XML (`16:433`)

  **What to do**:
  - Author `ui_pro/screens/berhasil.xml`.
  - Content: 100px success circle containing a 60px checkmark icon; "Scan Berhasil!" heading; a `card_bg` pill/panel row with "ID Pasien:" (18px, `text_secondary`) + the patient ID value (30px, `text_primary`) — **ID must be a settable property**, the Figma `P-2026-259` is illustrative only; a `card_bg` hint panel "Tekan START untuk mulai pengukuran".
  - ButtonBar slots: `Start / Restart / Power / Menu`.

  **References**:
  - Figma dark node `16:433` — ScanScreen at x=112 y=64, success circle 100px, Frame 10 at y=132, hint container at y=258
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → Copy section, Berhasil strings

  **Blocked By**: 8, 9, 10 · **Blocks**: 23

- [ ] 18. Select Age screen XML (`56:1789`)

  **What to do**:
  - Author `ui_pro/screens/age.xml`.
  - Content: "Pilih Rentang Usia" title (22px) at y≈52; four selectable option rows, each 400×69 at x=40 (y = 88, 169, 250, 331), each containing a primary label (22px) and a secondary descriptor (23px line-height, `text_secondary`): `6-17 Tahun` / "Anak-anak & Remaja", `18-45 Tahun` / "Dewasa", `46-60 tahun` / "Dewasa Tua", `>60 tahun` / "Lansia".
  - Option rows must be **focusable** so the keypad group (Task 14) can move focus with Up/Down. Ensure `LV_STATE_FOCUSED` is visually obvious on dark background.
  - ButtonBar slots: `Up / Down / Back / Select`.

  **Must NOT do** (in addition to shared rules):
  - Do not implement the selection→session write here (Task 24 wires it).

  **References**:
  - Figma dark node `56:1789` — exact row geometry (400×69 at x=40, y offsets 88/169/250/331) and label casing (note `Tahun` vs `tahun` differs per row — copy verbatim)
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → Age bands, and the `LV_STATE_FOCUSED` visibility requirement

  **Blocked By**: 8, 9, 10 · **Blocks**: 23

- [ ] 19. Select Gender screen XML (`60:290`)

  **What to do**:
  - Author `ui_pro/screens/gender.xml`.
  - Content: "Pilih Jenis Kelamin" title (22px) at y≈52; two selectable rows 400×84 at x≈40 (y = 88 and 186), each with a 48px icon container (24px glyph inside) at x=18 and a 22px label at x=78: "Laki-Laki", "Perempuan".
  - Rows must be focusable for keypad Up/Down, same focus-visibility requirement as Age.
  - ButtonBar slots: `Up / Down / Back / Select`.

  **References**:
  - Figma dark node `60:290` — row geometry 400×84, icon container 48px, label offset x=78
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → Gender strings; `AGENTS.md` for the backend `M`/`F`/`U` mapping (used later in Task 24, not in XML)

  **Blocked By**: 8, 9, 10 · **Blocks**: 23

- [ ] 20. Mengukur screen XML (`36:1446`)

  **What to do**:
  - Author `ui_pro/screens/mengukur.xml`.
  - Content: "Mengukur..." heading (36px) + "Mohon tunggu, jangan gerakkan sensor" (21px, `text_secondary`); a progress bar 400×8 at x=20 with an `accent` fill plus a percentage label below (both **settable properties**); a 2×2 grid of large vital cards (Task 11) at y≈140, each 214×88: "SpO2" + value + "%", "Laju Pernapasan" + value + "/min", "Detak Jantung" + value + "bpm", "Tekanan Darah" + "106/81"-style value + "mmHg".
  - ButtonBar slots: `Abort / [empty] / Power / Menu`.

  **References**:
  - Figma dark node `36:1446` — MeasuringScreen at x=20 y=64, progress at y=79, vital grid at y=140 with 214×88 cells
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → Mengukur strings (note the Indonesian vital labels differ from Result's abbreviations)

  **Blocked By**: 8, 9, 10, 11 · **Blocks**: 23

- [ ] 21. Scan Result screen XML (`16:1008`)

  **What to do**:
  - Author `ui_pro/screens/result.xml`.
  - Content: a large result banner 440×225 (radius 10) whose **fill color is a settable property** (RED confirmed `#fb2c36`; YELLOW/GREEN/BLACK fills are NOT in Figma — expose the property and use a documented placeholder, flagging that the designer must confirm the other three); inside: a white 64px circle with a 38px warning icon, the triage label (24px bold white, e.g. "MERAH - IMMEDIATE" — settable), and a translucent pill showing "ID Pasien: <id>" (settable).
  - Below: a 4-up compact vital card row (Task 11) at y=244, each 104×89: SpO2 %, HR bpm, RR /min, BP mmHg — all values settable.
  - ButtonBar slots: `Monitor / Reset / Power / Menu`.

  **Must NOT do** (in addition to shared rules):
  - Do not invent final YELLOW/GREEN/BLACK banner colors as if confirmed — mark them as placeholders pending design confirmation.

  **References**:
  - Figma dark node `16:1008` — full design context captured this session: banner 440×225 `#fb2c36`, white circle 64px, vital row 4×104×89 at y=244
  - `AGENTS.md` → triage label mapping (RED→"MERAH - IMMEDIATE" etc.) and the note that only RED is color-confirmed

  **Blocked By**: 8, 9, 10, 11 · **Blocks**: 23

- [ ] 22. Monitor screen XML (`63:378`)

  **What to do**:
  - Author `ui_pro/screens/monitor.xml`.
  - Content: a patient-ID row at y=60 (12px dot + ID label, settable); a large SpO2 card 440×142 at y=93 (24px icon + "SpO2" label, huge 56px value + "%" unit, plus a 4px progress/level bar); a 2-up row at y=247 of 214×93 cards for HR ("bpm") and RR ("/min"); a footer strip 440×37 at y=352 with a live dot + "Monitoring aktif" (left) and a small clock icon + "Update 5s lalu" (right, settable).
  - All vital values must be settable properties (Task 25 drives them live from the mock).
  - ButtonBar slots: `Back / Stop / Power / Menu`.

  **References**:
  - Figma dark node `63:378` — full design context captured this session: SpO2 card 440×142 at y=93 (value 56px), HR/RR cards 214×93 at y=247, footer at y=352
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → Monitor strings ("Monitoring aktif", "Update 5s lalu")

  **Blocked By**: 8, 9, 10, 11 · **Blocks**: 23

- [ ] 23. Export all screens to `ui/generated/` + commit policy

  **Execution note**: The export is a **user GUI step** (Editor "Compile & export code" across the whole project). The agent verifies the exported output, commits it, and re-checks both builds.

  **What to do (user, in Editor)**:
  - With all 8 screens + 3 components authored, click **"Compile & export code"** to generate C for the whole project into `ui/generated/`.

  **What the agent does**:
  - Confirm the generated `triagebox_ui_init()` entry point and per-screen `*_create()` functions exist in the exported output.
  - Commit ALL generated `*_gen.c` / `*_gen.h` (per the Key Decision). Keep the one-time user skeleton files distinct from `_gen` files.
  - Add `ui/generated/README.md` stating "generated by LVGL Pro Editor (Compile & export code) — do not edit; re-export from the Editor and commit."
  - Re-confirm both `sim` and `idf.py build` compile with the full generated set (Task 8 proved one screen; this is all of them).

  **Must NOT do**:
  - Agent must not attempt `lved-cli.js` — export is GUI-only.
  - Do not hand-edit generated files.
  - Do not gitignore the generated files.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Agent side is verify + commit + rebuild once the user has exported.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: NO — join point after all screens
  - **Parallel Group**: Wave 4 start
  - **Blocks**: 24, 27, 28
  - **Blocked By**: 15-22

  **References**:
  - <https://lvgl.io/docs/pro/integration/using-exported-c-code> — `_gen` vs user files, init entry point, Editor export button
  - `docs/ui-workflow.md` (from Task 8) — the exact Editor export steps to repeat

  **Acceptance Criteria**:
  - [ ] `ui/generated/` contains gen C/H for all 8 screens + 3 components
  - [ ] `triagebox_ui_init` symbol present
  - [ ] Both targets still build

  **QA Scenarios**:

  ```
  Scenario: All screens exported and both targets build
    Tool: Bash
    Preconditions: user has exported the full project from the Editor
    Steps:
      1. `ls ui/generated/*_gen.c | wc -l` — assert >= 11 (8 screens + 3 components)
      2. `grep -rl "triagebox_ui_init" ui/generated/` — assert found
      3. `cmake --build sim/build` — exit 0; `idf.py build` — exit 0
    Expected Result: full generation present, both builds green
    Failure Indicators: missing gen files, build breakage with full set
    Evidence: .sisyphus/evidence/task-23-generate.txt

  Scenario: Generated files are committed, not ignored
    Tool: Bash
    Preconditions: generation done
    Steps:
      1. `git check-ignore ui/generated/*_gen.c` — assert NOTHING is ignored (empty output, exit 1)
      2. `git status --porcelain ui/generated/` shows tracked files
    Expected Result: generated C is tracked
    Failure Indicators: files ignored or untracked
    Evidence: .sisyphus/evidence/task-23-committed.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-23-generate.txt`, `task-23-committed.txt`

  **Commit**: YES
  - Message: `chore(ui): export all screens to generated C`
  - Files: `ui/generated/*`
  - Pre-commit: both targets build

- [x] 24. `ui_nav.c` + `ui_action.c` — flow graph and per-screen action tables

  **What to do**:
  - Create `ui/logic/ui_nav.h/.c`: a screen enum + `ui_nav_go(screen)` using `lv_screen_load()` on the permanent screens created by `triagebox_ui_init()`. Encode the flow graph and the Key Decision transitions exactly:
    - Home Scan → Scanning; Scanning (mock RFID) → Berhasil; Scanning Abort → Home
    - Berhasil Start → Age; Berhasil Restart → Scanning (clear RFID)
    - Age Select → Gender; Age Back → Berhasil
    - Gender Select → Mengukur; Gender Back → Age
    - Mengukur (timer done) → Result; Mengukur Abort → Home
    - Result Monitor → Monitor; Result Reset → Home (clear session)
    - Monitor Back → Result; Monitor Stop → Result
    - Power / Menu (any screen) → no-op + log
  - Create `ui/logic/ui_action.h/.c`: one action table per screen mapping button index 0-3 → handler, plus a single `ui_action(screen, btn_id)` entry point. **Both** the ButtonBar touch events and the keypad `LV_KEY_*` events route through this one function (no dual logic paths).
  - For Age/Gender, use the `lv_group_t` from Task 14 for Up/Down focus movement; Select reads the focused option into the session (Task 12); Back/Select still go through the action table.
  - Wire Age/Gender selections to `ui_session_set_age/gender`; wire Berhasil to store the mock RFID; wire Reset/Home to `ui_session_reset()`.

  **Must NOT do**:
  - Do not create two separate handlers for touch vs button — exactly one `ui_action`.
  - Do not add screens/transitions beyond the table above.
  - Do not put navigation logic inside generated XML files.

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: The state machine is the correctness core; dual-input unification and focus handling need careful design.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: NO — integration hub
  - **Parallel Group**: Wave 4
  - **Blocks**: 25, 26, 27
  - **Blocked By**: 12, 14, 23

  **References**:
  - `Key Decisions` table in this plan — the authoritative transition list (Abort/Restart/Reset/Stop/Back targets)
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → "Input" section: one action table per screen, touch+keypad share handlers; focus only for Age/Gender
  - `ui/logic/ui_session.*` (Task 12), `ui/logic/ui_input.*` (Task 14)

  **WHY Each Reference Matters**:
  - The Key Decisions table removes all ambiguity about where each button goes — implement it literally, do not reinterpret.

  **Acceptance Criteria**:
  - [ ] Single `ui_action()` reached by both touch and keypad (verified by grep: no second dispatch path)
  - [ ] Every transition in the Key Decisions table implemented
  - [ ] Power/Menu are logged no-ops

  **QA Scenarios**:

  ```
  Scenario: Touch and keypad converge on one action dispatcher
    Tool: Bash
    Preconditions: ui_action.c written
    Steps:
      1. `grep -rn "ui_action(" ui/logic sim` — confirm both the ButtonBar event cb and the keypad key handler call ui_action
      2. Assert there is no second per-screen switch duplicating handlers elsewhere
    Expected Result: one dispatcher, two call sites (touch + keypad)
    Failure Indicators: duplicated handler logic, screen-specific button code outside the action tables
    Evidence: .sisyphus/evidence/task-24-single-dispatch.txt

  Scenario: Full flow graph transitions are all present
    Tool: Bash
    Preconditions: ui_nav.c written
    Steps:
      1. For each transition in the Key Decisions table, grep ui_nav.c for the source→target mapping
      2. Assert all are present; assert Power/Menu map to a no-op/log function
    Expected Result: every documented transition encoded
    Failure Indicators: missing transition, Power/Menu doing something other than no-op
    Evidence: .sisyphus/evidence/task-24-transitions.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-24-single-dispatch.txt`, `task-24-transitions.txt`

  **Commit**: YES (groups with 25)
  - Message: `feat(ui): wire navigation, action tables, live vitals`
  - Files: `ui/logic/ui_nav.{h,c}`, `ui/logic/ui_action.{h,c}`
  - Pre-commit: sim builds

- [ ] 25. Wire mock vitals into Mengukur / Result / Monitor

  **What to do**:
  - Add an LVGL timer (created after `triagebox_ui_init`) that calls `ui_mock_tick()` and pushes values into the visible screen via the generated screens' setter functions / the components' property setters — using **plain C setters**, under `lvgl_port_lock` on device (in sim, from the single LVGL loop, no lock needed).
  - Mengukur: advance the progress bar from 0→100% over `UI_MEASURE_MS`, then trigger the nav transition to Result (via `ui_nav_go`) and set the mock priority on the session.
  - Result: render the session's priority (banner color + label + ID) and the captured vitals in the 4 compact cards.
  - Monitor: update HR/SpO2/RR (and the "Update Ns lalu" footer) on each mock tick with slight jitter so it looks live.
  - Provide a debug key (documented) that cycles the mock priority so QA can view RED/YELLOW/GREEN/BLACK Result states.

  **Must NOT do**:
  - Do not compute priority from vitals (no rule logic — that is ML, out of scope). Priority comes from the mock cycle only.
  - Do not update LVGL objects from outside the LVGL loop without the lock (keep the firmware-correct pattern even in sim).
  - Do not hardcode values into generated files.

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Timer/update wiring touching three screens with correct LVGL threading discipline.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4
  - **Blocks**: 26
  - **Blocked By**: 13, 24

  **References**:
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` → "Vital display and waveform" (timer-driven updates, not per-sample) and "Threading" (lock rules)
  - `ui/logic/ui_mock.*` (Task 13), `ui/logic/ui_session.*` (Task 12)

  **WHY Each Reference Matters**:
  - The skill file's threading section is the exact rule set that keeps this code correct when it later runs on the device with `esp_lvgl_port`.

  **Acceptance Criteria**:
  - [ ] Mengukur progress reaches 100% then auto-navigates to Result
  - [ ] Result shows the mocked priority + vitals
  - [ ] Monitor values visibly update over time
  - [ ] Priority-cycle debug key shows all 4 Result variants

  **QA Scenarios**:

  ```
  Scenario: Measure completes and lands on Result with mocked priority
    Tool: Bash
    Preconditions: sim built, UI_MEASURE_MS accelerated
    Steps:
      1. Launch sim, navigate Home→...→Mengukur via documented keys
      2. Wait > UI_MEASURE_MS
      3. Assert (log/screenshot) the active screen is Result and shows a valid triage label from the allowed set
    Expected Result: auto-transition to Result with mock priority rendered
    Failure Indicators: stuck on Mengukur, no transition, blank priority
    Evidence: .sisyphus/evidence/task-25-measure-to-result.png

  Scenario: All four triage variants are viewable
    Tool: Bash
    Preconditions: sim on Result, priority-cycle debug key known
    Steps:
      1. Press the cycle key 3 times, screenshotting each state
      2. Assert labels cycle through MERAH/KUNING/HIJAU/HITAM variants
    Expected Result: all four Result states render
    Failure Indicators: a variant missing or crashing
    Evidence: .sisyphus/evidence/task-25-priority-cycle.png

  Scenario: Monitor values update live
    Tool: Bash
    Preconditions: sim on Monitor
    Steps:
      1. Screenshot Monitor at t=0 and t=3s
      2. Assert at least one vital value differs (jitter) and footer "Update" text updated
    Expected Result: visible live update
    Failure Indicators: frozen values
    Evidence: .sisyphus/evidence/task-25-monitor-live.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-25-measure-to-result.png`, `task-25-priority-cycle.png`, `task-25-monitor-live.txt`

  **Commit**: YES (groups with 24)
  - Message: `feat(ui): wire navigation, action tables, live vitals`
  - Files: `ui/logic/ui_runtime.{h,c}` (or similar timer glue)
  - Pre-commit: sim builds and runs

- [ ] 26. `sim/` full run — all screens navigable via keys

  **What to do**:
  - Finalize `sim/main.c` to: init LVGL + SDL display/mouse/keyboard, create the keypad indev (Task 14), call `triagebox_ui_init("")`, start the runtime timer (Task 25), load Home, and run the loop.
  - Confirm the full documented key sequence walks every screen and returns to Home.
  - Print an `lv_mem_monitor` / heap summary after all screens are created (the RAM-budget sanity check flagged by Sun Tzu — informational, logged to evidence).
  - Add a short "how to drive the sim" section to `docs/ui-workflow.md` (key map, flow).

  **Must NOT do**:
  - Do not add features not in the 8 screens.
  - Do not require a physical board.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: End-to-end integration + scripted runtime verification.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4
  - **Blocks**: 28, F3
  - **Blocked By**: 24, 25

  **References**:
  - <https://lvgl.io/docs/open/9.3/details/integration/ide/pc-simulator> — the run loop
  - `docs/ui-workflow.md` — key map defined in Task 14

  **Acceptance Criteria**:
  - [ ] Documented key sequence visits all 8 screens and returns to Home without crash
  - [ ] Heap summary printed after screen creation
  - [ ] Sim survives a 30s idle on Monitor without leak/crash

  **QA Scenarios**:

  ```
  Scenario: Full 8-screen walk via keys
    Tool: Bash
    Preconditions: sim built
    Steps:
      1. Launch sim under a driver that sends the documented key sequence: Home Scan, (mock rfid), Start, (age Down/Select), (gender Select), wait measure, Monitor, Back, Reset
      2. Screenshot at each screen; assert 8 distinct screens observed in order and final screen is Home
    Expected Result: complete traversal, ends on Home, no crash
    Failure Indicators: stuck screen, crash (exit 139/134), wrong order
    Evidence: .sisyphus/evidence/task-26-full-walk/ (8 screenshots + log)

  Scenario: No leak/crash on idle Monitor
    Tool: Bash
    Preconditions: sim on Monitor
    Steps:
      1. `timeout 30 ./sim/build/triagebox_sim` driven to Monitor then idle
      2. Assert process still alive at timeout (rc 124), heap summary shows no unbounded growth between two prints
    Expected Result: stable, no runaway allocation
    Failure Indicators: crash, monotonically rising heap use
    Evidence: .sisyphus/evidence/task-26-idle-heap.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-26-full-walk/` screenshots, `task-26-idle-heap.txt`

  **Commit**: YES (groups with 27)
  - Message: `test(ui): verify sim run and ESP-IDF build`
  - Files: `sim/main.c`, `docs/ui-workflow.md`
  - Pre-commit: full walk passes

- [ ] 27. `main/` app_main links full `ui/` + green ESP-IDF build

  **What to do**:
  - Extend `main/app_main.c` to call `triagebox_ui_init("")` and load Home through `esp_lvgl_port` (init port, then the same runtime timer), guarded so it builds even without real panel bring-up — the goal remains **`idf.py build` succeeds** with the full UI linked, not a lit panel.
  - Ensure the shared `ui/generated/` and `ui/logic/` compile as an IDF component with no `#ifdef` platform hacks inside those shared files (platform glue stays in `main/`).
  - Confirm no LVGL v8 symbols and LVGL 9.x resolved.

  **Must NOT do**:
  - Do not implement CH32/GT911/RGB bring-up.
  - Do not add `#ifdef ESP_PLATFORM` inside `ui/generated` or `ui/logic`.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Cross-target link correctness; catching platform-leak ifdefs.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 4
  - **Blocks**: 28
  - **Blocked By**: 23, 24

  **References**:
  - `AGENTS.md` → build section + `esp_lvgl_port` ownership rules (no own tick/handler)
  - <https://lvgl.io/docs/pro/integration/using-exported-c-code> — init/load sequence

  **Acceptance Criteria**:
  - [ ] `idf.py build` exits 0 with full `ui/` linked
  - [ ] No platform ifdefs in `ui/generated` or `ui/logic`
  - [ ] LVGL 9.x resolved

  **QA Scenarios**:

  ```
  Scenario: Full UI links under ESP-IDF
    Tool: Bash
    Preconditions: ui/generated + ui/logic complete
    Steps:
      1. `idf.py set-target esp32s3 && idf.py build` — assert exit 0
      2. `grep -rn "triagebox_ui_init" build/ || true` and assert the symbol was compiled/linked
    Expected Result: green build with UI linked
    Failure Indicators: unresolved symbols, LVGL v8 pulled, build error
    Evidence: .sisyphus/evidence/task-27-idf-full-build.txt

  Scenario: Shared UI code stays platform-neutral
    Tool: Bash
    Preconditions: build done
    Steps:
      1. `grep -rn "ESP_PLATFORM\|#ifdef ESP\|SDL" ui/generated ui/logic`
      2. Assert zero matches (platform glue must live in sim/ and main/ only)
    Expected Result: no platform leakage in shared code
    Failure Indicators: any platform ifdef in shared dirs
    Evidence: .sisyphus/evidence/task-27-no-platform-leak.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-27-idf-full-build.txt`, `task-27-no-platform-leak.txt`

  **Commit**: YES (groups with 26)
  - Message: `test(ui): verify sim run and ESP-IDF build`
  - Files: `main/app_main.c`, `main/CMakeLists.txt`
  - Pre-commit: `idf.py build` exits 0

- [ ] 28. Update AGENTS.md + `docs/ui-workflow.md`

  **What to do**:
  - Update `AGENTS.md` to reflect the actual workflow now in place:
    - Screens are authored in LVGL Pro XML under `ui_pro/`, exported to `ui/generated/` (committed, never hand-edited), logic in `ui/logic/`.
    - Amend the "Code conventions" line that says "one screen = one module under `main/ui/screens/`" — it's now `ui_pro/screens/*.xml` → generated C; hand-written C is logic only.
    - Note the two build targets (`sim/`, `main/`) and that shared `ui/` must stay platform-neutral.
    - Add the `*_gen` no-edit rule and the commit-generated-C policy.
  - Finalize `docs/ui-workflow.md`: Editor install/license (Community, no CLI), the manual edit → **"Compile & export code"** (Editor GUI) → commit workflow, how to run the sim (key map, flow), how to build for ESP-IDF, and the "add a new screen" checklist (author in Editor → export → agent wires logic).
  - Keep the existing skills accurate — if any statement in `.agents/skills/lvgl-triagebox-ui/SKILL.md` now conflicts (e.g. implies hand-written screens), reconcile it.

  **Must NOT do**:
  - Do not document features not built (light theme, serial, ML) as if present.
  - Do not remove the hardware/data-contract sections of AGENTS.md — only the UI-authoring convention changes.

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation reconciliation.
  - **Skills**: none

  **Parallelization**:
  - **Can Run In Parallel**: NO — last
  - **Parallel Group**: Wave 4 end
  - **Blocks**: F1
  - **Blocked By**: 23, 26, 27

  **References**:
  - `AGENTS.md` current "Code conventions" + "UI contract" sections
  - `.agents/skills/lvgl-triagebox-ui/SKILL.md` — reconcile any hand-written-screen implication

  **Acceptance Criteria**:
  - [ ] AGENTS.md describes the Pro XML → generated C → logic layout
  - [ ] `*_gen` no-edit rule present
  - [ ] `docs/ui-workflow.md` has install, edit/export, sim-run, idf-build, add-screen sections
  - [ ] No conflicting "hand-written screen module" statement remains

  **QA Scenarios**:

  ```
  Scenario: AGENTS.md reflects the new workflow and no stale convention remains
    Tool: Bash
    Preconditions: docs updated
    Steps:
      1. `grep -n "ui_pro\|ui/generated\|do not edit\|_gen" AGENTS.md` — assert present
      2. `grep -n "one screen = one module under .main/ui/screens" AGENTS.md` — assert the stale line is gone or amended
    Expected Result: new workflow documented, stale convention removed
    Failure Indicators: contradictory conventions coexisting
    Evidence: .sisyphus/evidence/task-28-agents.txt

  Scenario: Workflow doc is complete and self-consistent
    Tool: Bash
    Preconditions: docs/ui-workflow.md written
    Steps:
      1. `grep -niE 'license|validate|generate|simulator|idf.py build|add a new screen' docs/ui-workflow.md`
      2. Assert all required sections present
    Expected Result: all sections found
    Failure Indicators: missing section
    Evidence: .sisyphus/evidence/task-28-workflow-doc.txt
  ```

  **Evidence to Capture**:
  - [ ] `task-28-agents.txt`, `task-28-workflow-doc.txt`

  **Commit**: YES
  - Message: `docs: update AGENTS.md and add UI workflow guide`
  - Files: `AGENTS.md`, `docs/ui-workflow.md`, `.agents/skills/lvgl-triagebox-ui/SKILL.md`
  - Pre-commit: grep checks pass

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to the user and get an explicit "okay" before completing.
>
> **Do NOT auto-proceed after verification.** Never mark F1-F4 checked before the user's okay.

- [ ] F1. **Plan compliance audit** — `oracle`
  Read this plan end-to-end. For each "Must Have": verify it exists (read file, run command). For each "Must NOT Have": grep the tree for the forbidden pattern and reject with file:line if found (serial framing, LoRa, MQTT, C5.0, light theme, 9th screen, `LV_USE_XML=1`, GPIO button reads). Confirm evidence files exist under `.sisyphus/evidence/`.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [ ] F2. **Code quality review** — `unspecified-high`
  Build both targets. Review every hand-written file (exclude `*_gen.*`) for: dead code, empty error paths, `printf` debris left in logic, unused includes, magic numbers that belong in tokens, duplicated touch-vs-keypad handlers. Verify no LVGL v8 symbols and no raw hex in `ui/logic`.
  Output: `sim build [PASS/FAIL] | idf build [PASS/FAIL] | Files [N clean/N issues] | VERDICT`

- [ ] F3. **Real manual QA** — `unspecified-high`
  From a clean build, execute EVERY QA scenario from EVERY task. Then run the full flow end-to-end in one session: Home → Scanning → Berhasil → Age → Gender → Mengukur → Result → Monitor → back to Home. Test all four triage colors, every Abort/Restart/Reset/Stop/Back path, rapid repeated keypresses, and empty ButtonBar cells. Save to `.sisyphus/evidence/final-qa/`.
  Output: `Scenarios [N/N pass] | Flow [PASS/FAIL] | Edge cases [N tested] | VERDICT`

- [ ] F4. **Scope fidelity check** — `deep`
  For each task: read "What to do", read the actual diff. Verify 1:1 — nothing missing, nothing beyond spec. Check "Must NOT do" compliance per task. Flag cross-task contamination (a screen task touching logic files) and any unaccounted files.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

| Task(s) | Message | Notes |
| --- | --- | --- |
| 1-2 | `chore(ui): scaffold LVGL Pro project and dark tokens` | |
| 3 | `feat(ui): add mock data type contracts` | |
| 4-5 | `chore(ui): add subset fonts and icon assets` | binary assets |
| 6-7 | `chore(build): add SDL simulator and ESP-IDF targets` | |
| 8 | `feat(ui): prove XML to C export across both targets` | |
| 9-11 | `feat(ui): add StatusBar, ButtonBar, VitalCard components` | |
| 12-14 | `feat(ui): add session model, mock provider, keypad indev` | |
| 15-22 | `feat(ui): add {screen} screen` | one commit per screen |
| 23 | `chore(ui): export all screens to generated C` | |
| 24-25 | `feat(ui): wire navigation, action tables, live vitals` | |
| 26-27 | `test(ui): verify sim run and ESP-IDF build` | |
| 28 | `docs: update AGENTS.md and add UI workflow guide` | |

---

## Success Criteria

### Verification Commands
```bash
cmake -S sim -B sim/build && cmake --build sim/build   # exit 0
timeout 10 ./sim/build/triagebox_sim                    # stays up, boots Home
idf.py set-target esp32s3 && idf.py build               # exit 0
git grep -nE '#[0-9a-fA-F]{6}' ui/logic sim main        # no output
git grep -c 'lv_indev_drv_t\|lv_disp_drv_t'             # 0
ls ui/generated/*_gen.c                                  # present and committed
```

### Final Checklist
- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent
- [ ] Both targets build clean
- [ ] All 8 screens navigable in sim via documented keys
- [ ] AGENTS.md reflects the Pro workflow and the `*_gen` rule
