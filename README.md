# TriageBox Node UI (LVGL)

Firmware antarmuka layar untuk **node TriageBox** — alat triase cerdas PKM-KC 2026 untuk korban bencana gempa. Berjalan di Waveshare **ESP32-S3-Touch-LCD-4** (480×480), menampilkan alur triase START dan monitoring tanda vital, dapat dioperasikan **penuh lewat 4 tombol fisik** maupun sentuh.

Bagian dari sistem TriageBox:

```
[ STM32 ] --serial (RS485)--> [ ESP32-S3 ] --LoRa--> Station --Ethernet--> Backend + Dashboard
 sensor filter                 UI + ML triase
 4 tombol fisik                ^ repo ini
 RFID
```

Satu node punya **dua MCU**:

- **STM32** — filter sensor, baca 4 tombol fisik, baca RFID
- **ESP32-S3** (repo ini) — UI layar, **inferensi triase (Decision Tree C5.0)**, kirim LoRa

Keduanya tersambung lewat **link serial** (kandidat utama RS485 via transceiver SP3485 onboard di `GPIO43`/`GPIO44`; framing internal bebas). Vital, event tombol, dan RFID mengalir dari STM32 ke ESP32.

Karena inferensi jalan di ESP32, node inilah yang **menghasilkan** `priority`, `confidence`, dan `reasons` — bukan meneruskan dari STM32. Ini yang membuat sistem tetap jalan saat jaringan lumpuh: keputusan triase diambil lokal, bukan di server.

Repo terkait (sibling folder): `triagebox-backend` (NestJS + MQTT + PostgreSQL), `triagebox-dashboard` (Next.js command center).

## Peran repo ini

- Alur UI triase: scan RFID → input usia/jenis kelamin (opsional) → pengukuran 1 menit → hasil kategori → monitoring
- **Inferensi triase lokal** (Decision Tree C5.0) dari vital yang diterima STM32 → `priority` / `confidence` / `reasons`
- Monitoring tanda vital near-realtime (HR, SpO₂, RR, tekanan darah, baterai)
- Indikator status sistem, sensor, dan tautan LoRa
- Navigasi tombol fisik (event dari STM32) + touch
- Framing serial ↔ STM32 dan paket LoRa menuju station (payload vital mengikuti kontrak MQTT backend)

**Bukan** milik repo ini: driver sensor (MAX30102 / AD8232 / MPX5010DP), debounce tombol fisik, reader RFID (itu semua di STM32), training model C5.0 (offline / Colab), backend MQTT.

## Hardware

Board: **Waveshare ESP32-S3-Touch-LCD-4**, modul ESP32-S3-WROOM-1-**N16R8**.

| Item | Nilai |
| --- | --- |
| Display | ST7701, 480×480, RGB565 16-bit parallel + 3-wire SPI kontrol |
| Pixel clock | 16 MHz (default BSP resmi) |
| Touch | GT911, I²C, alamat `0x5D` (fallback `0x14`) |
| I²C bersama | SDA `GPIO15`, SCL `GPIO7` |
| Flash / PSRAM | 16 MB QIO / 8 MB **octal (OPI)** @ 80 MHz |
| Framebuffer | wajib di PSRAM |
| RTC | PCF85063A @ `0x51` |
| microSD | SDMMC 1-bit: CLK `GPIO2`, CMD `GPIO1`, D0 `GPIO4` |

Wiki resmi: <https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4>
Demo resmi: <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4>

### Revisi board menentukan driver

Board ini punya beberapa revisi dengan chip pembantu berbeda. **Cek silkscreen sebelum menulis kode.**

| Revisi | Chip pembantu | Alamat I²C |
| --- | --- | --- |
| V4.0 (jalur resmi saat ini) | CH32V003F4U6 | `0x24` |
| V1–V3 | TCA9554PWR | — |

Pada V4, sinyal berikut **bukan GPIO ESP32** melainkan lewat CH32 via I²C: `LCD_RST` (EXIO3), `TP_RST` (EXIO1), backlight (PWM), buzzer `BEE_EN` (EXIO6), `SYS_EN` (EXIO5), ADC baterai, `RTC_INT` (EXIO7). Layar gelap biasanya berarti init CH32 gagal, bukan panel rusak.

### Link ke STM32 dan tombol fisik

4 tombol fisik **tidak** tersambung ke GPIO ESP32 — tombol dibaca STM32 lalu dikirim sebagai event lewat link serial. Ini menguntungkan, karena board ini praktis **tidak punya GPIO bebas**: hampir semua pin terpakai RGB, SPI, USB, I²C, SD, CAN, dan RS485.

| Kandidat link | Pin | Catatan |
| --- | --- | --- |
| **RS485** (rencana utama) | `GPIO43` RX / `GPIO44` TX | Transceiver SP3485 onboard, auto TX/RX switching, tahan noise + jarak |
| UART TTL langsung | `GPIO43`/`GPIO44` | Pin sama, tanpa transceiver — cukup bila kedua MCU satu board |
| CAN | `GPIO0`/`GPIO6` | Transceiver TJA1051 onboard; `GPIO0` pin strapping, hati-hati |

Karena `GPIO43`/`GPIO44` dipakai untuk link STM32, pin tersebut **bukan lagi kandidat bebas** untuk keperluan lain. Verifikasi dengan skematik revisi board fisik sebelum menyolder. Lihat `AGENTS.md` untuk daftar GPIO terpakai.

## Toolchain

| Komponen | Versi |
| --- | --- |
| ESP-IDF | ≥ 5.3.1 (CI resmi: 5.5.x / 6.0.x) |
| LVGL | v9 (demo resmi menyediakan 8.4 dan 9.5) |
| BSP | `waveshare/esp32_s3_touch_lcd_4` 3.0.0 |

## Build & flash

```bash
idf.py set-target esp32s3
idf.py menuconfig      # pastikan PSRAM octal + flash 16MB
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

Flash dan log memakai port USB-C yang sama (USB native ESP32-S3). Bila gagal flash: tutup serial monitor, tahan **BOOT** saat menyalakan, flash, lalu power-cycle.

## Desain

Figma (dua varian tema, 480×480, 8 layar):

- Dark: <https://www.figma.com/design/etAAzsnQu0RlnxnPYNBEJz/UI-UX?node-id=53-1781>
- Light: <https://www.figma.com/design/etAAzsnQu0RlnxnPYNBEJz/UI-UX?node-id=208-5>

Setiap layar memakai struktur sama: status bar 480×48 di atas, konten di tengah, **ButtonBar 480×71** di bawah berisi 4 tombol 120×71 yang labelnya memetakan langsung ke 4 tombol fisik.

| Layar | Label 4 tombol (kiri → kanan) |
| --- | --- |
| Home | — / Scan / Power / Menu |
| Scanning RFID | Abort / — / Power / Menu |
| Scan Berhasil | Start / Restart / Power / Menu |
| Pilih Usia | Up / Down / Back / Select |
| Pilih Jenis Kelamin | Up / Down / Back / Select |
| Mengukur | Abort / — / Power / Menu |
| Hasil Triase | Monitor / Reset / Power / Menu |
| Monitor | Back / Stop / Power / Menu |

Teks UI berbahasa Indonesia, mengikuti desain.

## Kategori triase

Sesuai START dan enum backend: `RED` (Immediate), `YELLOW` (Delayed), `GREEN` (Minor), `BLACK` (Expectant). Layar hasil menampilkan label Indonesia, mis. "MERAH - IMMEDIATE".

## Dokumentasi terkait

- `AGENTS.md` — batasan hardware, GPIO terpakai, kontrak data untuk agen/kontributor
- `../triagebox-backend/docs/api-contract.md` — kontrak MQTT/REST/WS lengkap
