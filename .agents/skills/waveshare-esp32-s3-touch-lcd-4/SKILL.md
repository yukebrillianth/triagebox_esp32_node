---
name: waveshare-esp32-s3-touch-lcd-4
description: Board bring-up for Waveshare ESP32-S3-Touch-LCD-4 (N16R8). Use when initializing display/touch, wiring buttons or UART, flashing, debugging a blank screen, or touching any GPIO/I2C expander code.
---

# Waveshare ESP32-S3-Touch-LCD-4

Module: ESP32-S3-WROOM-1-**N16R8** (16 MB flash, 8 MB octal PSRAM).

- Wiki: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4
- Demo repo (V4 path): https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4
- Managed BSP: `waveshare/esp32_s3_touch_lcd_4` v3 — prefer this over hand-rolled ST7701/GT911 init

## Revision gate (do this first)

| Silkscreen | Helper chip | Notes |
| --- | --- | --- |
| **V4.0** (current) | **CH32V003F4U6 @ I²C 0x24** | Official maintained software path |
| V1–V3 / no print | **TCA9554PWR** | Different EXIO numbering — do not mix |

Confirm the silkscreen under the screen flex before writing any expander code.

## Display

| Item | Value |
| --- | --- |
| Panel | ST7701, 480×480, 65K |
| Pixel bus | RGB565 16-bit parallel |
| Control bus | 3-wire SPI: CS `GPIO42`, SCL `GPIO2`, SDA `GPIO1` |
| Timing pins | PCLK `41`, HSYNC `38`, VSYNC `39`, DE `40` |
| Pixel clock | **16 MHz** (current BSP); older wiki tip mentioned 21 MHz |
| Framebuffer | **must be in PSRAM** (`.flags.fb_in_psram = 1`) |

RGB data map (DATA0…15 → GPIO): `5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17`.

## Touch

| Item | Value |
| --- | --- |
| Chip | GT911 |
| Bus | I²C SDA `GPIO15`, SCL `GPIO7` |
| Address | `0x5D` primary, `0x14` backup |
| INT | wiki maps `GPIO16`; official BSP sets `GPIO_NUM_NC` (poll mode) |
| RST | **via expander**, not a direct ESP32 GPIO |

## Shared I²C bus (GPIO15/7)

Devices: GT911, PCF85063A RTC @ `0x51`, CH32V003 @ `0x24` (V4), external I2C header.

After a soft reset, run **I²C bus recovery** before installing the driver — ESP32 can reboot while a slave still holds SDA low. Official examples do this first.

## V4 CH32 expander map

These are **not** ESP32 GPIOs:

| CH32 signal | Function |
| --- | --- |
| EXIO1 | TP_RST |
| EXIO3 | LCD_RST |
| EXIO5 | SYS_EN (power hold) |
| EXIO6 | BEE_EN (buzzer) |
| EXIO7 | RTC_INT (input) |
| PWM API | backlight |
| ADC API | battery voltage |

Dark screen with a live USB log almost always means CH32 init failed, not a dead panel.

### Older TCA9554 map (do not use on V4)

EXIO0=TP_RST, EXIO1=BL_EN, EXIO2=LCD_RST, EXIO3=SD_CS, EXIO4=BLC, EXIO5=BEE_EN, EXIO6=RTC_INT, EXIO7=DO1.

## ESP32 GPIO budget

**Consumed always:** `1–5, 7–15, 17–21, 38–42, 45–48` (RGB + SPI + USB + I2C + SD).

**Conditional:** `0,6` (CAN), `43,44` (RS485), `16` (TP_INT, BSP-unused).

**4 physical buttons live on the STM32, not on ESP32.** No free GPIO bank is needed on this board for buttons.

Dual-MCU link (planned): **RS485** on `GPIO43` (RX) / `GPIO44` (TX) via the onboard SP3485 transceiver. Those pins are **in use** for the STM32 link — do not reassign them for other peripherals. Alternatives if the plan changes: bare UART TTL on the same pins, or CAN on `GPIO0`/`GPIO6` (avoid `GPIO0` as a strap pin).

Button events and vitals arrive as serial frames from the STM32. ESP32 only maps them into the UI / LoRa path.

## sdkconfig (N16R8)

```
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y
```

Current demos use **80 MHz** PSRAM. The wiki's 120 MHz tip is older/experimental — do not enable it unless you know why.

## Toolchain

| Component | Version |
| --- | --- |
| ESP-IDF | ≥ 5.3.1 (CI: 5.5.x / 6.0.x) |
| Arduino-ESP32 | wiki 3.0.7; CI 3.3.11 |
| LVGL | Arduino demos 8.4; ESP-IDF demos 8.4 **and 9.5** |
| BSP | `waveshare/esp32_s3_touch_lcd_4` 3.0.0 |

This project targets **LVGL v9**. Pin it in `idf_component.yml`:

```yaml
dependencies:
  waveshare/esp32_s3_touch_lcd_4: "^3.0.0"
  lvgl/lvgl: "^9.5.0"
  espressif/esp_lvgl_port: "^2.8.0"
```

## Flash / boot gotchas

1. USB-C is native ESP32-S3 USB — flash and monitor share one port. Close the serial monitor before flashing.
2. Crash loop → hold **BOOT** while powering on, flash, then power-cycle out of download mode.
3. Battery power: press **BAT_PWR / PWRKEY** to turn on; double-click to turn off. V4 holds power via CH32 `SYS_EN`.
4. Some USB-C PD ports can damage the board's TVS/MOS (wiki documents rework) — prefer a simple 5 V supply when debugging.
5. SW6106 light-load auto-off and I²C interference with wide-voltage DC are real; prefer battery or disable SW6106 I²C if touch dies under DC supply.
6. Prefer ESP-IDF CLI over the VS Code plugin compile path for this board (wiki: pixel offset / ghosting with the plugin).

## Bring-up order

1. Confirm silkscreen revision → pick CH32 or TCA path
2. Enable OPI PSRAM + 16 MB flash in sdkconfig
3. I²C bus recovery → install bus on 15/7
4. Init expander → LCD_RST, TP_RST, backlight on
5. ST7701 RGB panel with `fb_in_psram`
6. GT911 (poll mode unless you re-enable INT deliberately)
7. LVGL via `esp_lvgl_port` (`lvgl_port_init` + `lvgl_port_add_disp_rgb`)
8. Only then: RS485/UART link to STM32 (vitals in, button events in) → feed keypad indev buffer

## Do not

- Wire 4 physical buttons to ESP32 GPIOs (they belong on the STM32)
- Free / reassign `GPIO43`/`GPIO44` once the RS485↔STM32 link is committed
- Mix TCA EXIO numbers into V4 CH32 code
- Drive backlight / LCD_RST / TP_RST as ESP32 GPIOs on V4
- Skip PSRAM (blank panel)
- Depend on TP_INT without re-enabling it in the BSP
