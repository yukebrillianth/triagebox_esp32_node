# AGENTS.md — triagebox-lvgl

Greenfield ESP-IDF + LVGL firmware for the **TriageBox node UI** on Waveshare ESP32-S3-Touch-LCD-4 (N16R8). Sibling repos: `../triagebox-backend`, `../triagebox-dashboard`.

Read this before writing any code. Prefer executable sources of truth (`mqtt-payload.ts`, `simulator/index.js`, board BSP) over prose.

## Scope of this repo

**In scope:** LVGL screens, status bar, theme dark/light, display of vitals + triage result, RS485 receive of vitals + button/RFID events from STM32, UI state machine of the triage flow, mapping STM32 button events → LVGL keypad indev, **on-device linear SVM inference** (runtime only), and sending the result back to the STM32 which owns the LoRa TX.

**Out of scope:** sensor drivers (MAX30102 / AD8232 / MPX5010DP / RFID live on STM32), physical button GPIO (STM32 owns them), **LoRa radio** (SX1278 hangs off the STM32 — no free SPI pins here), **training** of the SVM (offline), MQTT broker/publish (station owns that), dashboard. Do not invent sensor code here.

> **Note on the ML model:** the PKM proposal (§2.2) specifies Decision Tree C5.0 and explicitly rejects SVM on memory grounds. The implementation was changed to **linear SVM** by the team after that was written. The proposal text has not been updated — flag this if a reviewer compares them.

## Hardware traps (will waste hours if missed)

1. **Board revision is not optional.** V4.0 uses **CH32V003 @ I²C 0x24**. Older boards use **TCA9554**. Check silkscreen under the flex. Do not mix EXIO numbering across revisions. **This repo runs on V3.0 (TCA9554 @ 0x20)** — treat V4 notes as context only.
2. On V4, `LCD_RST`, `TP_RST`, backlight PWM, buzzer, `SYS_EN`, battery ADC are **not ESP32 GPIOs** — they go through the CH32. Dark screen = CH32 init failed. **On V3 there is no `SYS_EN` at all** (EXIO5 is `BLC`); the **SW6106 PMIC @ 0x3c** owns the battery and the rails, and `ui_board_power_off()` cuts power by writing to it.
3. **Framebuffer must live in PSRAM** (`fb_in_psram = 1`). OPI/octal PSRAM required. Without it the panel looks dead.
4. Shared I²C: SDA `GPIO15`, SCL `GPIO7`. **Verified by bus scan on V3.0: `0x20` TCA9554, `0x3c` SW6106, `0x51` PCF85063A, `0x5d` GT911.** The 10-pin `Interface` header also exposes SDA/SCL, so external devices can add addresses. `0x26` ACKs but refuses every transaction — a partial-decode phantom, not a second expander. QMI8658 is **NC**, so `0x6a`/`0x6b` are free. Run **I²C bus recovery** after a soft reset (SDA can stick low) — `bsp_i2c_bus_recover()` does this before `i2c_new_master_bus()`. **The more common stall is SCL, not SDA**: measured SCL LOW 4/4 after soft resets with the STM32 attached, i.e. its slave clock-stretching forever. No master can clear that — power-cycle, and see `docs/firmware-architecture.md`. Re-scan any time with the `i2c` console command.
5. **Dual-MCU link consumes RS485 pins.** Planned path is serial to STM32 over RS485 on `GPIO43` (RX) / `GPIO44` (TX) via the onboard **MAX13487EESA+** (U7) — an *AutoDirection* transceiver, so there is **no DE/RE pin to drive** and plain UART mode is correct. Those pins are **not free** for other uses once the link is wired.
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
idf.py flash monitor    # port auto-detect; -p COM7 (Win) / -p /dev/cu.usbmodem* (mac) to force
```

> **`ui_board_power_off()` really cuts power** — it writes the SW6106's shutdown
> register and the board dies, **even over USB**. There is no dry-run mode, so
> anything that must survive has to be persisted first. Sequence and rationale:
> `docs/firmware-architecture.md` §Power off.

Required sdkconfig shape (N16R8):

```
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
```

Prefer the managed BSP `waveshare/esp32_s3_touch_lcd_4` over hand-rolled ST7701/GT911 init — but **pick the version by silkscreen revision, not by "latest"**. The physical board here is **rev 3.0** → BSP `^1.1.0` (TCA9554 expander). BSP `^2`/`3.x` targets HW **V4.0** (CH32V003 @ 0x24, different EXIO numbering). Official demo repo: <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4>. Wiki: <https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4>.

BSP 1.1.0 does **not** compile on ESP-IDF v6 as shipped: v6 removed `psram_trans_align` and `bits_per_pixel` from `esp_lcd_rgb_panel_config_t`, and the registry manifest claims `idf: >=5.3` with no upper bound so the solver never catches it. Workaround in tree: the component is vendored to `components/esp32_s3_touch_lcd_4/` (overrides the managed copy) with those two fields deleted, plus the 180° panel/touch mirror for the enclosure, plus `touch_read_tolerant()` — a touch read callback that replaces the port's, because `esp_lvgl_port` `ESP_ERROR_CHECK`s `esp_lcd_touch_read_data()` and one GT911 timeout on the shared bus therefore `abort()`ed the firmware (that was the "blackscreen, backlight lit" bug; see `docs/firmware-architecture.md`). Keep all three — do not "upgrade" to `^3.0.0`.

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

## Dual-MCU I²C contract (STM32 ↔ ESP32)

**Implemented** in `components/triagebox_link/` — `tb_regs.h` is the authoritative register map and is a **verbatim copy** of the STM32 project's file; edit one, copy it over. `TB_PROTO_VER` (read first, at reg `0x00`) makes a stale copy fail loudly instead of misreading offsets. Full detail: `docs/firmware-architecture.md`.

ESP32-S3 is I²C **master**, STM32F411 is slave at **0x42**, on the display's existing bus (SDA GPIO15 / SCL GPIO7). No extra pins, no transceiver, and the read-only `i2creg` / `i2cdump` / `i2craw` console commands inspect the STM32 like any other chip on that bus.

```
read : S 0x42 W [reg] Sr 0x42 R [d0] [d1] ... P     (snapshot, pointer auto-increments)
write: S 0x42 W [reg] [d0] [d1] ... P
```

The read block is latched when the master addresses the slave for reading, so one multi-byte read can never mix an old HR with a new SpO2. Little-endian. Layout, per-vital validity bits, the button **state** mask (the ESP32 diffs it into edges — see `tb_i2c_codec.c`) and the command register all live in `tb_regs.h`; do not restate offsets anywhere else.

`RESULT` (priority + confidence + tag) goes back through the command/result registers. Poll cadence is 50 ms from the LVGL timer.

**RS485 is superseded.** `tb_link.c` (UART2, `0xA5 0x5A` framing, CRC-16/CCITT-FALSE) is kept for one release in case the swap has to be reverted; the STM32 never had a USART, so I²C is the only transport that has ever had two ends. `tb_frame.c` stays regardless — the LoRa payload still uses its priority conversion, and it is host-tested.

`priority` on the wire uses the **LoRa numeric alias** (`0=BLACK, 1=RED, 2=YELLOW, 3=GREEN`), which is not `ui_priority_t` order. Always go through `tb_frame_priority_to_wire()` / `_from_wire()`. `tb_frame.c` has no malloc and no ESP-IDF dependency so the STM32 side can compile it verbatim.

**ML ownership:** STM32 never sends `priority` / `confidence` / `reasons`. ESP32 runs the SVM on the received vitals and sends `RESULT` back; **the STM32 owns the LoRa TX** and forwards it to the station. Station-side MQTT still uses the canonical JSON in the next section.

### On-device inference constraints

- Model is a **pre-trained linear SVM** (one-vs-rest, 4 classes) compiled into firmware as `static const float` weights in `components/triagebox_ml/include/tb_svm_model.h`. Training lives offline (Colab / host) — not on-device. **The committed model is a zero placeholder: it classifies everything RED.**
- Inputs: hr, spo2, rr, bp_sys, bp_dia. Age band + gender are collected by the UI for victim registration but are **not** model features.
- Outputs **must** match backend contract: `priority` ∈ `RED|YELLOW|GREEN|BLACK`, `confidence` float 0–1 (or 0–100, backend normalizes), `reasons` string array (e.g. `"HR>130"`, `"SpO2<90"`).
- `reasons` is sent **empty**: an SVM has no rule path to quote and no separate threshold table was added. The backend defaults `reasons` to `[]`, so this is valid.
- Invalid/stale vitals (`flags` bit0 clear) return BLACK with confidence 0 rather than guessing.
- Keep it tiny — ESP32-S3 has headroom, but do not pull a general ML framework. A dot product over 5 floats is enough; `tb_svm_classify()` is ~40 flops and allocation-free.
- Inference runs once when the measure window ends (`ui_runtime.c` → `pull_mock_priority_once`), and sends `RESULT` in the same call. It is cheap enough to run on the LVGL task; do not add a task for it.

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
| `main/` | ESP-IDF app (bring-up only: `app_main.c`, `asset_fs.c`) |
| `components/esp32_s3_touch_lcd_4/` | Vendored BSP: IDF-v6 patch + 180° panel/touch mirror + non-fatal touch read. Overrides the managed copy. |
| `components/triagebox_link/` | RS485 ↔ STM32: `tb_frame.c` codec (platform-neutral), `tb_link.c` UART2, `tb_ui_source.c` |
| `components/triagebox_board/` | Backlight + buzzer via TCA9554, and `ui_board_power_off()` via the SW6106 PMIC. `test_fakes/` lets the host selftest compile the real file. |
| `components/triagebox_debug/` | `CONFIG_TB_DEBUG_CONSOLE` REPL: frame injection + I²C scan/read (`i2c`, `i2creg`, `i2craw`, `i2cdump`) + `stats`. Off by default. |
| `components/triagebox_ml/` | Linear SVM inference + the exported model header |
| `tools/run_selftests.sh` | Compiles and runs every `*_selftest.c` on the host under ASan/UBSan |

`ui_mock.h` has **two** implementations, selected in CMake rather than by `#ifdef`: `ui/logic/ui_mock.c` (sim, deterministic fake) and `components/triagebox_link/tb_ui_source.c` (device, RS485 + SVM). Anything added to that header must be implemented in both. See `docs/firmware-architecture.md`.

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
