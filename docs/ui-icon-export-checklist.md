# TriageBox Icon Export Checklist

Snapshot baseline: commit `598a25d` (`07/08/2026`)
Source of truth: Figma file `etAAzsnQu0RlnxnPYNBEJz`, dark nodes.

## Export Rules

- Canvas: `24 x 24 px` for every icon unless noted.
- Stroke: normally `2 px`, round cap and round join.
- Export with transparent background and alpha; do not bake screen/card colors into navigation icons.
- Use one monochrome source per semantic role. Apply LVGL recolor for white, accent, muted, or danger states.
- Button cell: `120 x 71 px`; icon box at local `(48, 12)` with `24 x 24 px`; label baseline around local `y=40`.
- Status bar icon box: `24 x 24 px`; status bar is `480 x 48 px`.
- Generated `*_gen.c` and `file_list_gen.cmake` must be refreshed through LVGL Pro export after asset changes. Do not hand-edit generated files.

## Shared Status Bar

| Role | Asset to export | Box | Color/state |
|---|---|---:|---|
| Battery normal | `battery_full` | 24x24 | `#00D460`, current ~80% |
| Battery medium | `battery_medium` | 24x24 | `#00D460`, medium level |
| Battery empty/low | `battery_empty` | 24x24 | `#00D460`, low level |
| Link connected | `signal_connected` | 24x24 | `#00D460`, four ascending bars |
| Link weak | `signal_weak` | 24x24 | `#00D460` or muted state from Figma/runtime decision |
| Link disconnected | `signal_zero` | 24x24 | muted/danger state from runtime decision |
| Clock | none | text only | `09:14` is text; no clock icon in the status bar |

Current `icon_signal` is the connected state. `icon_wifi_high/low/zero` are candidates for runtime states but must not be deleted until the status-bar state API is finalized.

## Bottom Bar Icons, All Screens

All entries below are `24x24`, white/alpha source, `2 px` stroke unless Figma export says otherwise.

| Semantic role | Used on | Color |
|---|---|---|
| Scan/RFID start | Home cell 1 | `#FFFFFF` |
| Abort/close X | Scanning and Mengukur cell 0 | `#FB2C36` |
| Start/play | Berhasil cell 0 | `#FFFFFF` |
| Restart/rotate counter-clockwise | Berhasil cell 1, Result cell 1 | `#FFFFFF` |
| Up chevron | Age and Gender cell 0 | `#FFFFFF`; dim only if first item is disabled in the actual state |
| Down chevron | Age and Gender cell 1 | `#FFFFFF`; dim only if last item is disabled in the actual state |
| Back full left arrow | Age, Gender, Monitor | `#FFFFFF` |
| Select/check | Age and Gender cell 3 | `#FFFFFF` |
| Monitor/eye or live-monitor glyph | Result cell 0 | `#FFFFFF`; verify the exact Figma asset, do not use signal bars |
| Stop square-in-circle | Monitor cell 1 | `#FB2C36` |
| Power | every populated Power cell | icon and label `#FB2C36` |
| Menu hamburger | every populated Menu cell | `#FFFFFF` |

ButtonBar maps exactly:

- Home: empty / Scan / Power / Menu
- Scanning: Abort / empty / Power / Menu
- Berhasil: Start / Restart / Power / Menu
- Age: Up / Down / Back / Select
- Gender: Up / Down / Back / Select
- Mengukur: Abort / empty / Power / Menu
- Result: Monitor / Reset / Power / Menu
- Monitor: Back / Stop / Power / Menu

## Age Screen

- No content icon is present in the Figma Age cards.
- Four cards: `400 x 69 px`, x=`40`, y=`88, 169, 250, 331`.
- Selected card: `#00D460` background, both title and subtitle `#FFFFFF`.
- Unselected card: `#1A2651` background, title `#FFFFFF`, subtitle `#99A1AF`.
- Bottom icons:
  - Up: `24x24`, white chevron-up, stroke `2 px`.
  - Down: `24x24`, white chevron-down, stroke `2 px`.
  - Back: `24x24`, white full left arrow, stroke `2 px`.
  - Select: `24x24`, white check, stroke `2 px`.
- Current XML uses arrow assets for all four. Choose one consistent Up/Down family and use it for both Age and Gender; Figma visual is a chevron, not a long shaft arrow.

## Gender Screen

- Male row: `400 x 84 px`, x=`40`, y=`88`; icon badge `48 x 48` at local `(18,18)`; symbol canvas `24 x 24` centered in badge.
- Female row: `400 x 84 px`, x=`40`, y=`186`; icon badge `48 x 48` at local `(18,18)`; symbol canvas `24 x 24` centered in badge.
- Badge: white circle. Male/female symbol is dark navy on unselected and accent/white according to the selected-state treatment in the latest manual export.
- Selected row: `#00D460` background. Unselected row: `#1A2651` background.
- Replace current placeholders:
  - `icon_lock` is wrong for Male.
  - `icon_bluetooth` is wrong for Female.
- Bottom icons are identical to Age: Up / Down / Back / Select, all `24x24`; use the exact manual export for both screens.

## Mengukur / Result / Monitor Vital Icons

| Vital | Asset role | Box | Color |
|---|---|---:|---|
| SpO2 | waveform/oxygen | 24x24 (drawn glyph ~20-26) | `#51A2FF` |
| HR | heart pulse | 24x24 | `#FB2C36` |
| RR | airflow/respiration | 24x24 | `#53EAFD` |
| BP | gauge/blood pressure | 24x24 | `#A78BFA` |

Mengukur uses four cards around `200x88` in the current XML, while Figma's measured cards differ by screen. Treat the manually corrected Mengukur export as the visual reference and fix layout separately from icon export.

Result warning asset:

- White priority badge: `64x64` in Figma Result.
- Warning triangle glyph: about `39x38` inside the badge.
- RED state glyph color: `#FB2C36`; badge fill `#FFFFFF`; banner fill `#FB2C36`.
- Current `icon_info` is only a generic stand-in and should be replaced by the manually exported warning asset.

Monitor update icon:

- Update/upload glyph: `12x12`, accent green `#00D460`.
- Monitoring status dot: about `6-7 px`, accent green.
- These are content icons, not ButtonBar icons.

## Safe Cleanup Candidates

Static XML references show these generic symbols are not used by the eight triage screens:

`icon_arrow_right`, `icon_battery`, `icon_battery_full`, `icon_bell`, `icon_calendar`, `icon_camera`, `icon_chevron_left`, `icon_chevron_right`, `icon_download`, `icon_edit`, `icon_heart`, `icon_home`, `icon_mail`, `icon_minus`, `icon_moon`, `icon_plus`, `icon_settings`, `icon_sun`, `icon_trash`, `icon_unlock`, `icon_upload`, `icon_user`, `icon_volume`, `icon_wifi`.

Do not delete yet:

- `icon_wifi_high`, `icon_wifi_low`, `icon_wifi_zero`: likely future link states.
- battery variants: required by status-bar runtime state.
- all `vec_*` assets: they are still declared by the Figma Flow export and require a full Editor re-export before cleanup.
- any `*_gen.c`/`*_gen.h`: generated files are never hand-edited or manually pruned.
- template component directories until LVGL Pro export regenerates `file_list_gen.cmake`; otherwise the build can reference deleted generated sources.

Cleanup sequence:

1. Replace/manual-export the missing semantic icons, especially Gender, Age/Gender bottom bar, Result warning, and Result Monitor.
2. Remove unused declarations and source assets in the Editor/source XML project.
3. Run full LVGL Pro `Compile & export code`.
4. Confirm generated file list no longer contains removed assets/components.
5. Rebuild simulator and ESP-IDF.

## Known Visual Problems Beyond Icons

The current slicing/export still needs separate layout passes for Scanning, Berhasil, Result, and Monitor. Icon cleanup alone will not fix card geometry, spacing, typography, gradients, or content positioning.

## Unused Template Components (defer to Editor export)

These stock LVGL Pro template components are not referenced by any of the 8 screens
(static tag scan). They are safe to DELETE only inside the Editor, followed by a full
`Compile & export code` so `file_list_gen.cmake` no longer lists their `*_gen.c`.
Do NOT delete them by hand now — the build compiles every generated source in the file list.

Unused now: arc, bar, slider, switch, checkbox, dropdown, text_box, list, list_item,
list_section, list_separator, h1, h2, h3, h4.

Keep (directly referenced or transitive base): base_box, container, panel, row, column,
image, monoicon, h5, text, button, status_bar, button_bar, vital_card. Keep
keyboard/text_input only if a future text-input screen needs them; neither is used by the
current eight triage screens.
