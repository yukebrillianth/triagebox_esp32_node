---
name: lvgl-triagebox-ui
description: Screen map, button-bar contract, design tokens, and LVGL v9 patterns for the TriageBox node UI (480x480). Use when implementing or modifying any screen, widget, theme, or input handling in triagebox-lvgl.
---

# TriageBox node UI

480×480, LVGL v9, ESP-IDF. Figma file `etAAzsnQu0RlnxnPYNBEJz` is the source of truth. Dark section `53:1781`, light section `208:5`.

Pull real design context with the Figma MCP (`get_design_context` on a screen node) instead of guessing values. Never hand-write a screen from a screenshot alone.

## Layout skeleton (identical on every screen)

```
y=0    StatusBar   480×48   battery% | link state | clock
y=48   content     480×361
y=409  ButtonBar   480×71   4 × 120×71 cells
```

StatusBar: left group at x=20 (battery icon 24px + "80%", then link icon + "Connected"), clock right-aligned at x=397. Text 16px semibold; clock regular.

ButtonBar cell: icon 24×24 at top (y=12), label 16px semibold centered below (y=40). Empty cells exist — render the cell background but no icon/label.

## Screen map and node IDs

| Screen | Dark | Light | Buttons (1→4) |
| --- | --- | --- | --- |
| Home | `16:98` | `208:6` | — / Scan / Power / Menu |
| Scanning RFID | `16:302` | `208:78` | Abort / — / Power / Menu |
| Scan Berhasil | `16:433` | `208:138` | Start / Restart / Power / Menu |
| Select Age | `56:1789` | `208:200` | Up / Down / Back / Select |
| Select Gender | `60:290` | `208:257` | Up / Down / Back / Select |
| Mengukur | `36:1446` | `208:511` | Abort / — / Power / Menu |
| Scan Result | `16:1008` | `208:318` | Monitor / Reset / Power / Menu |
| Monitor | `63:378` | `208:402` | Back / Stop / Power / Menu |

Flow: Home → Scanning RFID → Scan Berhasil → Select Age → Select Gender → Mengukur → Scan Result → Monitor.

"Power" label is always red (`#fb2c36`) in both themes.

## Design tokens

Put these in `main/ui/theme/`. No raw hex in screen modules.

| Token | Dark | Light |
| --- | --- | --- |
| `screen_bg` | `#0d1329` | `#fefefe` |
| `card_bg` | `#1a2651` | `#e5f1f9` |
| `border` | `#1a2651` | `#e2e5e8` |
| `text_primary` | `#ffffff` | `#34383f` |
| `text_secondary` | `#99a1af` | `#99a1af` |
| `text_on_card` | `#d1d5dc` | `#031e3c` |
| `accent` | `#00d460` | `#16bc4e` |
| `status_ok` | `#00c950` | `#16bc4e` |
| `danger` | `#fb2c36` | `#fb2c36` |
| `buttonbar_cell` | gradient `#000827` → `#1a2651` (180°) | `#ffffff` |

Triage colors: RED `#fb2c36`. YELLOW / GREEN / BLACK are not in the Figma frames — confirm with the designer before inventing them.

Typography: Inter. Sizes seen — 48 bold (logo), 24 bold (result), 18 bold/semibold, 16 semibold (bar + status), 14 regular, 13, 10, 9. Card radius `10px`; pill radius `100px`; status dots fully round 12px.

## Copy (Bahasa Indonesia, exact)

- Home: "TriageBox", "Sistem Triase Cerdas", "Siap untuk memulai triase pasien bencana", "Tekan **START** untuk memindai gelang RFID pasien", "Sistem OK", "Sensor OK", "LoRa OK"
- Scanning: "Memindai RFID...", "Dekatkan gelang pasien ke sensor"
- Berhasil: "Scan Berhasil!", "ID Pasien:", "Tekan START untuk mulai pengukuran"
- Age: "Pilih Rentang Usia" — `6-17 Tahun`/Anak-anak & Remaja, `18-45 Tahun`/Dewasa, `46-60 tahun`/Dewasa Tua, `>60 tahun`/Lansia
- Gender: "Pilih Jenis Kelamin" — "Laki-Laki", "Perempuan"
- Mengukur: "Mengukur...", "Mohon tunggu, jangan gerakkan sensor", + progress %
- Result: "MERAH - IMMEDIATE", "ID Pasien: …"; vital cards labeled "SpO2 %", "HR bpm", "RR /min", "BP mmHg"
- Monitor: "Monitoring aktif", "Update 5s lalu"; labels "SpO2", "HR", "RR" with units "%", "bpm", "/min"

Measuring screen shows 4 vitals with Indonesian labels: "SpO2", "Laju Pernapasan", "Detak Jantung", "Tekanan Darah".

## Input: touch + 4 physical buttons (via STM32)

One action table per screen. Touch and buttons call the **same** handlers.

```c
typedef struct {
    const char *label;          /* NULL = empty cell */
    const void *icon;
    void (*action)(void);       /* NULL = disabled */
} ui_bar_slot_t;
```

**The 4 physical buttons are wired to the STM32, not ESP32 GPIOs.** STM32 debounces and sends press/release events over the dual-MCU serial link (planned RS485 on `GPIO43`/`GPIO44`). ESP32 serial RX task stores the latest button state in a shared buffer; the LVGL keypad `read_cb` only reads that buffer.

Physical buttons are a `LV_INDEV_TYPE_KEYPAD` indev (not `LV_INDEV_TYPE_BUTTON` — that one fakes touch at fixed coordinates and breaks when layout shifts). **Do not call `gpio_get_level()` for these four keys.**

Key mapping: `LV_KEY_PREV` / `LV_KEY_NEXT` / `LV_KEY_ENTER` / `LV_KEY_ESC`.

```c
lv_indev_t *indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
lv_indev_set_read_cb(indev, keypad_read_cb);  /* reads STM32 button buffer */
lv_indev_set_group(indev, group);
```

`read_cb` sets `data->key` and `data->state` (PRESSED/RELEASED); keep the last key while reporting RELEASED. Debounce on the STM32 — never call LVGL from a UART ISR; only update the shared buffer.

For list screens (age, gender) use `lv_group_t` focus: `lv_group_create`, `lv_group_add_obj`, `lv_indev_set_group`. Return `LV_KEY_NEXT` from the callback and let LVGL move focus; don't call `lv_group_focus_next()` from the read callback.

For the fixed 4-cell bar, dispatch each button straight to that screen's action table — do not route the bar through focus.

Screen-level Back: subscribe to `LV_EVENT_KEY` on the indev and check `lv_indev_get_key() == LV_KEY_ESC`. `LV_KEY_ESC` is consumed by some widgets, so it does not pop your screen stack automatically.

`LV_STATE_FOCUSED` must be clearly visible in both themes — button-only operation cannot rely on touch feedback.

## Vital display and waveform

Use `lv_chart` (`CONFIG_LV_USE_CHART=y`) before reaching for `lv_canvas`.

```c
lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);   /* scrolling trace */
lv_chart_set_next_value(chart, series, value);                  /* already invalidates */
```

`lv_chart_set_next_value()` schedules its own redraw — do not call `lv_chart_refresh()` after it. `lv_chart_refresh()` is only for direct array mutation / `lv_chart_set_series_ext_y_array`. Never use `lv_refr_now()` for streaming.

Do not update the UI at the sensor sample rate. Sensor task → ring buffer → LVGL timer at 20–40 Hz → one batched update. When decimating, keep per-bucket min and max so peaks survive.

## Theming at runtime

Baseline widget states via `lv_theme_default_init(display, primary, secondary, dark, font)` + `lv_display_set_theme()`. That API only carries primary/secondary/dark/font, so layer the token table above as named persistent `lv_style_t` objects and call `lv_obj_report_style_change(NULL)` after switching.

Keep every `lv_style_t` alive as long as an object references it — never a stack local.

## Threading

Any LVGL call from a non-LVGL task must be wrapped:

```c
if (lvgl_port_lock(1000)) {
    lv_label_set_text(hr_label, buf);
    lvgl_port_unlock();
}
```

`lvgl_port_lock(0)` blocks indefinitely — it is not a try-lock. Keep critical sections short; never hold the lock across UART reads or queue waits.

## Do not

- Write LVGL v8 code (`lv_indev_drv_t`, `lv_disp_drv_t`, `lv_disp_draw_buf_t`) — this project is v9.
- Duplicate logic between touch handlers and button handlers.
- Scatter hex colors across screen modules.
- Invent Indonesian copy — copy Figma strings verbatim.
