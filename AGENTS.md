# AGENTS.md — triagebox-lvgl

Greenfield ESP-IDF + LVGL firmware for the **TriageBox node UI** on Waveshare ESP32-S3-Touch-LCD-4 (N16R8). Sibling repos: `../triagebox-backend`, `../triagebox-dashboard`.

Read this before writing any code. Prefer executable sources of truth (`mqtt-payload.ts`, `simulator/index.js`, board BSP) over prose.

## Scope of this repo

**In scope:** LVGL screens, status bar, theme dark/light, display of vitals + triage result, serial receive of vitals + button/RFID events from STM32, UI state machine of the triage flow, mapping STM32 button events → LVGL keypad indev, **on-device Decision Tree C5.0 inference** (runtime only), packaging of vital+priority for LoRa TX toward the station.

**Out of scope:** sensor drivers (MAX30102 / AD8232 / MPX5010DP / RFID live on STM32), physical button GPIO (STM32 owns them), **training** of the C5.0 model (offline), MQTT broker/publish (station owns that), dashboard. Do not invent sensor code here.

## Hardware traps (will waste hours if missed)

1. **Board revision is not optional.** V4.0 uses **CH32V003 @ I²C 0x24**. Older boards use **TCA9554**. Check silkscreen under the flex. Do not mix EXIO numbering across revisions.
2. On V4, `LCD_RST`, `TP_RST`, backlight PWM, buzzer, `SYS_EN`, battery ADC are **not ESP32 GPIOs** — they go through the CH32. Dark screen = CH32 init failed.
3. **Framebuffer must live in PSRAM** (`fb_in_psram = 1`). OPI/octal PSRAM required. Without it the panel looks dead.
4. Shared I²C: SDA `GPIO15`, SCL `GPIO7` — GT911 + PCF85063A + CH32 + external header all share it. Run **I²C bus recovery** after a soft reset (SDA can stick low).
5. **Dual-MCU link consumes RS485 pins.** Planned path is serial to STM32 over RS485 on `GPIO43` (RX) / `GPIO44` (TX) via the onboard SP3485. Those pins are **not free** for other uses once the link is wired. Confirm on the physical rev + schematic before committing.
6. **4 physical buttons live on the STM32**, not on ESP32 GPIOs. Do not invent free ESP32 pins for buttons. Button presses arrive as serial frames from the STM32 and feed the LVGL keypad indev `read_cb`.
7. Touch INT is wiki-mapped to `GPIO16` but official BSP leaves it `GPIO_NUM_NC` (poll mode). Do not depend on INT unless you re-enable it deliberately.
8. Flash + logs share the same USB-C. Close serial monitor before flashing. Hold BOOT on power-up if the board is crash-looping.

Reserved ESP32 GPIOs (do not reassign): `1,2,3,4,5,7,8,9,10,11,12,13,14,15,17,18,19,20,21,38,39,40,41,42,45,46,47,48`. Conditionally reserved: `0,6` (CAN), **`43,44` (RS485 ↔ STM32 — planned in use)**.

## Build

ESP-IDF is not on `PATH` by default. Source the export script in **every shell** before any `idf.py` call (verified working with **v6.0.2**):

```bash
source ~/.espressif/v6.0.2/esp-idf/export.sh
```

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <port> flash monitor
```

Required sdkconfig shape (N16R8):

```
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
```

Prefer the managed BSP `waveshare/esp32_s3_touch_lcd_4` over hand-rolled ST7701/GT911 init — but **pick the version by silkscreen revision, not by "latest"**. The physical board here is **rev 3.0** → BSP `^1.1.0` (TCA9554 expander). BSP `^2`/`3.x` targets HW **V4.0** (CH32V003 @ 0x24, different EXIO numbering). Official demo repo: <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4>. Wiki: <https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4>.

BSP 1.1.0 does **not** compile on ESP-IDF v6 as shipped: v6 removed `psram_trans_align` and `bits_per_pixel` from `esp_lcd_rgb_panel_config_t`, and the registry manifest claims `idf: >=5.3` with no upper bound so the solver never catches it. Workaround in tree: the component is vendored to `components/esp32_s3_touch_lcd_4/` (overrides the managed copy) with those two fields deleted, plus the 180° panel/touch mirror for the enclosure. Keep both — do not "upgrade" to `^3.0.0`.

Pin LVGL to v9 — the port component accepts `>=8,<10`, so an unpinned resolve can silently give you v8:

```yaml
dependencies:
  waveshare/esp32_s3_touch_lcd_4: "^1.1.0"   # HW rev3.0 / TCA9554 — NOT ^3.0.0
  lvgl/lvgl: "^9.5.0"
  espressif/esp_lvgl_port: "^2.8.0"
```

Reject any snippet using `lv_indev_drv_t`, `lv_disp_drv_t`, or `lv_disp_draw_buf_t` — those are v8 patterns.

`esp_lvgl_port` owns LVGL init, the LVGL task, and the tick timer. Do not add your own `lv_tick_inc()` or a second `lv_timer_handler()` loop. Always wrap LVGL object mutations from non-LVGL tasks with `lvgl_port_lock()` / `lvgl_port_unlock()`. Note `lvgl_port_lock(0)` blocks indefinitely — it is not a try-lock.

## UI contract (Figma is law)

- Canvas: **480×480**. Status bar 480×48 top. ButtonBar 480×71 bottom, 4 cells 120×71.
- Themes: dark (`node-id=53-1781`) and light (`node-id=208-5`) of file `etAAzsnQu0RlnxnPYNBEJz`. Both must work at runtime.
- Per-screen node IDs (dark / light): Home `16:98`/`208:6`, Scanning `16:302`/`208:78`, Berhasil `16:433`/`208:138`, Age `56:1789`/`208:200`, Gender `60:290`/`208:257`, Mengukur `36:1446`/`208:511`, Result `16:1008`/`208:318`, Monitor `63:378`/`208:402`.
- Tokens (author in LVGL Pro `ui/globals.xml`, no raw hex in hand-written logic):

  | Token | Dark | Light |
  | --- | --- | --- |
  | screen_bg | `#0d1329` | `#fefefe` |
  | card_bg | `#1a2651` | `#e5f1f9` |
  | text_primary | `#ffffff` | `#34383f` |
  | text_secondary | `#99a1af` | `#99a1af` |
  | accent | `#00d460` | `#16bc4e` |
  | danger (Power + RED) | `#fb2c36` | `#fb2c36` |

- UI copy is **Bahasa Indonesia** (match Figma strings exactly: "Memindai RFID…", "MERAH - IMMEDIATE", "Monitoring aktif", …).
- Every on-screen ButtonBar label maps 1:1 to a physical button. Touch and buttons must drive the **same** action handlers — never dual logic paths.
- Screens (order of flow): Home → Scanning RFID → Scan Berhasil → Select Age → Select Gender → Mengukur → Scan Result → Monitor.
- Age bands: `6-17`, `18-45`, `46-60`, `>60`. Gender: `Laki-Laki` / `Perempuan` (payload gender codes for backend: `M` | `F` | `U`).
- Triage display labels map to enum: RED→"MERAH - IMMEDIATE", YELLOW→"KUNING - DELAYED", GREEN→"HIJAU - MINOR", BLACK→"HITAM - EXPECTANT".

Physical button model: 4 keys on the **STM32**, context-dependent labels on the ESP32 ButtonBar. Prefer `LV_INDEV_TYPE_KEYPAD` + `lv_group_t` focus for list screens (age/gender); for fixed action bars, map each STM32 button event directly to the screen's action table (do not rely on focus for the bottom bar). The keypad `read_cb` reads the latest STM32 button state from a shared buffer filled by the serial RX task — never `gpio_get_level()` for the four keys.

## Dual-MCU serial contract (STM32 ↔ ESP32)

Framing is free to invent in this repo, but the **payload content** that eventually becomes MQTT vital must match the backend keys below. Suggested minimal frame kinds from STM32 → ESP32:

| Kind | Content |
| --- | --- |
| `VITAL` | hr, spo2, rr, bp_sys, bp_dia, battery, (optional ecg_status) — **no priority** |
| `BUTTON` | which of 4 keys pressed/released (debounce on STM32) |
| `RFID` | raw tag id when scanned |
| `STATUS` | sensor OK flags |

ESP32 → STM32 (as needed): start/stop measure, power, abort. Keep the wire compact (binary preferred over JSON on the node link).

**ML ownership:** STM32 never sends `priority` / `confidence` / `reasons`. ESP32 runs the C5.0 tree on the received vitals (+ age/gender if operator filled them), then attaches those fields before LoRa TX. Station-side MQTT still uses the canonical JSON in the next section.

### On-device inference constraints

- Model is a **pre-trained C5.0 decision tree** compiled into firmware (rules/thresholds as static data or generated C). Training lives offline (Colab / host) — not on-device.
- Inputs (proposal from PKM): hr, rr, bp_sys/bp_dia, spo2; optional age band + gender if collected on UI.
- Outputs **must** match backend contract: `priority` ∈ `RED|YELLOW|GREEN|BLACK`, `confidence` float 0–1 (or 0–100, backend normalizes), `reasons` string array (e.g. `"HR>130"`, `"SpO2<90"`).
- Keep the tree tiny — ESP32-S3 has headroom, but do not pull a general ML framework. Plain if/else or a generated rule table is enough.
- Inference runs after the 1-minute measure window (and can re-run on monitoring updates if priority may change). Always take the LVGL lock only for UI updates, never around the tree walk if it can block.

## Data contract with backend (do not invent names)

Authoritative sources (read them, do not paraphrase into new keys):

- `../triagebox-backend/src/common/mqtt-payload.ts`
- `../triagebox-backend/docs/api-contract.md`
- `../triagebox-backend/simulator/index.js` (closest reference emitter)

### Priority enum (exact)

```
RED | YELLOW | GREEN | BLACK
```

Numeric alias only if talking binary LoRa: `0=BLACK, 1=RED, 2=YELLOW, 3=GREEN`. Never use IMMEDIATE/DELAYED/MINOR/EXPECTANT as wire values.

### MQTT topics (station publishes these; node reaches station via LoRa)

```
triagebox/{station_id}/{node_id}/vital     QoS 1
triagebox/{station_id}/{node_id}/status
triagebox/{station_id}/status              retained LWT OFFLINE
```

Station/node IDs live **in the topic**, not inside vital JSON. Demo seed IDs: `st-01`/`st-02`, `node-01`…`node-10`. Pre-registration required — unknown IDs are dropped.

### Canonical vital JSON (snake_case)

```json
{
  "victim_rfid": "3021",
  "hr": 90,
  "spo2": 98,
  "rr": 18,
  "bp_sys": 120,
  "bp_dia": 80,
  "battery": 80,
  "priority": "GREEN",
  "confidence": 0.9,
  "reasons": [],
  "ts": "2026-07-25T00:00:00.000Z"
}
```

Accepted aliases (backend normalizes): `heart_rate`→`hr`, `respiratory_rate`→`rr`, `timestamp`→`ts`, `triage_level`→`priority`, `confidence` 0–100 → 0–1. Optional: `ecg_status`, `device_status`, `packet_counter`, `packet_version`. **`ecg_status` is a status string/number, not raw ECG samples.**

Node status JSON: `{ "status":"ONLINE"|"OFFLINE", "rssi"?, "snr"?, "battery"?, "firmware"?, "packet_count"? }`. RSSI/SNR belong here, not on vital.

Nominal cadence (proposal / simulator): vital **15 s**, node status **30 s**.

### Identity rules

- `victim_rfid` string | number | null → stored as string. Null does **not** create a victim.
- Non-null RFID auto-creates/upserts a victim on the backend.
- Stations/nodes are never created by MQTT.

This firmware talks to the STM32 over UART with an internal framing of your choosing, but anything that eventually becomes an MQTT vital **must** match the canonical keys above. Prefer emitting the canonical shape end-to-end so the station does zero renaming.

## Code conventions

### Layout (current)

| Path | Role |
| --- | --- |
| `ui/` | LVGL Pro Editor project: XML (`globals.xml`, `screens/*.xml`, `components/`) + exported C |
| `ui/logic/` | Hand-written C only: types, session, mock, input, nav, action, runtime (+ host selftests) |
| `sim/` | SDL2 desktop simulator (LVGL v9.5, 480×480) |
| `main/` | ESP-IDF app (build/link target; board bring-up is separate) |

Shared `ui/` and `ui/logic/` must stay platform-neutral (no `ESP_PLATFORM` / SDL ifdefs there). Platform glue lives in `sim/` and `main/` only.

### Screens: Pro XML, not hand modules

- Screens and shared widgets are authored as **LVGL Pro XML** under `ui/screens/` and `ui/components/`, then exported to C.
- There is **no** `main/ui/screens/` hand-written screen module tree. Do not invent one.
- Theme tokens live in `ui/globals.xml` (Editor). Hand-written logic never hardcodes style hex.

### Generated C rules

- Never hand-edit `*_gen.c` / `*_gen.h` (or other Pro-generated sources). Fix the XML, re-export.
- Commit generated C after a successful full export so sim/ESP builds work without the Editor.
- A **full** project export must produce at least: `ui/ui.h`, `ui/ui.c`, `ui/ui_gen.*`, `globals_gen.*` (when the Editor emits them), `file_list_gen.cmake`, and the per-screen/component `*_gen.*` listed there.
- Partial export is a known failure mode: `CMakeLists.txt` alone is not enough. The sim gates `HAS_UI` / `lib-ui` on **both** `ui/CMakeLists.txt` **and** `ui/ui.h`. Without `ui.h` the sim falls back to a welcome label.

### Logic layer (`ui/logic/`)

| Module | Job |
| --- | --- |
| `ui_types.h` | `vitals_t`, `btn_event_t`, `rfid_t`, priority/age/gender enums (serial-stable shapes) |
| `ui_session` | One triage session (RFID, age, gender, vitals, priority) |
| `ui_mock` | Non-blocking mock RFID/vitals/buttons + accelerated measure (`UI_MEASURE_MS`) |
| `ui_input` | LVGL keypad indev; reads `ui_mock_pop_button()` only (never GPIO for the 4 keys) |
| `ui_nav` | Screen id + go/show registry + RFID/measure transition hooks |
| `ui_action` | **Single** dispatcher: touch ButtonBar cell and keypad both call `ui_action(screen, btn_id)` |
| `ui_runtime` | Tick glue: mock → session fill → nav transitions (no LVGL, no sleep) |

Button/key mapping (left→right cells 0..3):

| Physical / mock btn | LV_KEY | Typical bar role |
| --- | --- | --- |
| 0 | `PREV` | Up / first action |
| 1 | `NEXT` | Down / second action |
| 2 | `ENTER` | Back / confirm-ish (per screen table) |
| 3 | `ESC` | Select / fourth action |

`ui_action_on_key()` reverses that map so Age/Gender keep Up/Down/Back/Select on buttons 0/1/2/3. Empty bar cells are no-ops. Power/Menu are no-op + log in v1 (no Menu screen).

Status today: **logic layer is done and selftestable on host**. The 8 triage screens, Figma tokens in `globals.xml`, and a complete Pro export are still **human/Editor work**. Do not claim screens are complete.

### General

- C (ESP-IDF style) for hand-written code.
- No `as any`-equivalent hacks: no silenced compiler warnings for type punning on packed LoRa/UART structs without a comment naming the wire layout.
- Do not add new dependencies when LVGL + ESP-IDF + the Waveshare BSP already cover the need.
- Keep diffs small. Greenfield is not a license to scaffold "for later".
- Workflow detail: `docs/ui-workflow.md`.
- UI handoff to the ESP32/STM32 devs: `docs/integration-esp32-stm32.md`, package built by `tools/make_handoff.sh`.

## Skills

Load these when the task matches:

- `.agents/skills/waveshare-esp32-s3-touch-lcd-4` — board bring-up, CH32 expander, pin map, flash gotchas
- `.agents/skills/lvgl-triagebox-ui` — screen map, button bar contract, theme switch, Figma mapping

## What not to do

- Do not invent MQTT field names or priority strings.
- Do not put raw ECG samples on the wire as `ecg_status`.
- Do not assume free GPIOs for buttons.
- Do not target LVGL v8 APIs if the project is on v9 (and vice versa) — match `idf_component.yml`.
- Do not commit secrets, binary firmware blobs, or `sdkconfig` with local absolute paths.
