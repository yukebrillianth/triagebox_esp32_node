# TriageBox Node Firmware (ESP32-S3)

Firmware node **TriageBox** — alat triase cerdas PKM-KC 2026 untuk korban bencana gempa. Berjalan di Waveshare **ESP32-S3-Touch-LCD-4** (480×480), menampilkan alur triase START dan monitoring tanda vital, dapat dioperasikan **penuh lewat 4 tombol fisik** maupun sentuh. ESP32 juga menjalankan inferensi triase lokal.

Bagian dari sistem TriageBox:

```
[ STM32 ] <--RS485--> [ ESP32-S3 ]        repo ini: UI + inferensi SVM
 sensor filter          UI + ML triase
 4 tombol fisik
 RFID RC522
 LoRa SX1278 --------> Station --Ethernet--> Backend + Dashboard
```

Satu node punya **dua MCU**:

- **STM32** — filter sensor, baca 4 tombol fisik, baca RFID, **dan transmit LoRa**
- **ESP32-S3** (repo ini) — UI layar + **inferensi triase (linear SVM)**

Keduanya tersambung **RS485** via transceiver MAX13487EESA+ onboard di `GPIO44` (TX) / `GPIO43` (RX), UART2 @115200. Vital, event tombol, dan RFID mengalir dari STM32 ke ESP32; hasil inferensi dikirim balik lewat frame `RESULT`, lalu STM32 yang meneruskan ke station via LoRa. Radio tidak di ESP32 karena budget GPIO board ini sudah habis. Wire format lengkap: **`docs/firmware-architecture.md`**.

Karena inferensi jalan di ESP32, node inilah yang **menghasilkan** `priority` dan `confidence` — bukan meneruskan dari STM32. Ini yang membuat sistem tetap jalan saat jaringan lumpuh: keputusan triase diambil lokal, bukan di server.

Repo terkait (sibling folder): `triagebox-backend` (NestJS + MQTT + PostgreSQL), `triagebox-dashboard` (Next.js command center).

## Peran repo ini

- Alur UI triase: scan RFID → input usia/jenis kelamin (opsional) → pengukuran 1 menit → hasil kategori → monitoring
- **Inferensi triase lokal** (linear SVM, 5 fitur vital) → `priority` / `confidence`
- Monitoring tanda vital near-realtime (HR, SpO₂, RR, tekanan darah, baterai)
- Indikator status sistem, sensor, dan tautan LoRa
- Navigasi tombol fisik (event dari STM32) + touch
- Codec frame RS485 ↔ STM32 (`components/triagebox_link/`, payload vital mengikuti kontrak MQTT backend)

**Bukan** milik repo ini: driver sensor (MAX30102 / AD8232 / MPX5010DP), debounce tombol fisik, reader RFID, **transmit LoRa** (itu semua di STM32), training model SVM (offline / Colab), backend MQTT.

> Model SVM yang ter-commit masih **placeholder nol** — semua pasien jadi RED. Ganti `components/triagebox_ml/include/tb_svm_model.h` dari notebook training sebelum dipakai sungguhan.

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

| Revisi | Chip pembantu | Alamat I²C | Power |
| --- | --- | --- | --- |
| V4.0 (jalur resmi saat ini) | CH32V003F4U6 | `0x24` | `SYS_EN` (EXIO5) |
| V1–V3 | TCA9554PWR | `0x20` | SW6106 PMIC `0x3c`, **tanpa `SYS_EN`** |

Pada V4, sinyal berikut **bukan GPIO ESP32** melainkan lewat CH32 via I²C: `LCD_RST` (EXIO3), `TP_RST` (EXIO1), backlight (PWM), buzzer `BEE_EN` (EXIO6), `SYS_EN` (EXIO5), ADC baterai, `RTC_INT` (EXIO7). Layar gelap biasanya berarti init CH32 gagal, bukan panel rusak.

> **Repo ini dipakai di board V3.0**, jadi tabel V4 di atas hanya konteks. Di V3 **tidak ada `SYS_EN`** — EXIO5 adalah `BLC`. Yang memegang rail adalah **SW6106** di `0x3c`; lihat `ui_board_power_off()`.

#### Peta alamat I²C V3.0 (hasil scan hardware, bukan tebakan)

Jalankan `i2c` di debug console untuk mengulang scan ini.

| Alamat | Device |
| --- | --- |
| `0x20` | TCA9554PWR I/O expander (A2:A0=000) — backlight + buzzer |
| `0x3c` | SW6106 PMIC — baterai + power off |
| `0x51` | PCF85063A RTC |
| `0x5d` | GT911 touch (bukan fallback `0x14`) |

Catatan: `0x26` ikut meng-ACK tapi menolak semua transaksi — itu **phantom** dari partial address decoding, bukan expander kedua. QMI8658 IMU **NC** (footprint ada, tidak dipasang), jadi `0x6a`/`0x6b` bebas. Header `Interface` 10-pin ikut mengekspos `SDA`/`SCL`, jadi device eksternal bisa nambah alamat di bus yang sama.

### Link ke STM32 dan tombol fisik

4 tombol fisik **tidak** tersambung ke GPIO ESP32 — tombol dibaca STM32 lalu dikirim sebagai event lewat link serial. Ini menguntungkan, karena board ini praktis **tidak punya GPIO bebas**: hampir semua pin terpakai RGB, SPI, USB, I²C, SD, CAN, dan RS485.

| Kandidat link | Pin | Catatan |
| --- | --- | --- |
| **RS485** (rencana utama) | `GPIO43` RX / `GPIO44` TX | Transceiver **MAX13487EESA+** onboard (U7), *AutoDirection* — arah diatur di dalam chip, tanpa pin DE/RE |
| UART TTL langsung | `GPIO43`/`GPIO44` | Pin sama, tanpa transceiver — cukup bila kedua MCU satu board |
| I²C | `GPIO15` SDA / `GPIO7` SCL | Bus bersama, keluar di header `Interface`. ESP32 jadi master; STM32 tidak bisa push tanpa diminta |
| CAN | `GPIO0`/`GPIO6` | Transceiver TJA1051 onboard; `GPIO0` pin strapping, hati-hati |

Karena `GPIO43`/`GPIO44` dipakai untuk link STM32, pin tersebut **bukan lagi kandidat bebas** untuk keperluan lain. Verifikasi dengan skematik revisi board fisik sebelum menyolder. Lihat `AGENTS.md` untuk daftar GPIO terpakai.

## Toolchain

| Komponen | Versi |
| --- | --- |
| ESP-IDF | ≥ 5.3.1 (dipakai di sini: 6.0.2) |
| LVGL | v9.5 |
| BSP | `waveshare/esp32_s3_touch_lcd_4` **^1.1.0** — board fisik rev **3.0** (TCA9554). Jangan `^3.0.0`, itu untuk HW V4.0 (CH32V003). BSP 1.1.0 perlu patch untuk IDF v6, sudah di-vendor ke `components/`. |

## Build & flash

ESP-IDF **tidak** ada di `PATH` secara default — source dulu di **setiap shell baru** (terverifikasi dengan v6.0.2):

```bash
source ~/.espressif/v6.0.2/esp-idf/export.sh
```

```bash
idf.py build                          # target esp32s3 sudah dipin di sdkconfig.defaults
idf.py flash monitor                  # port auto-detect
```

Kalau auto-detect gagal (beberapa port serial sekaligus), sebutkan port-nya: `-p /dev/cu.usbmodem*` di macOS, `-p COM7` di Windows. **Lewat extension ESP-IDF di VS Code tidak perlu ini** — port dipilih di status bar bawah dan disimpan sebagai `idf.port` di `.vscode/settings.json` (folder itu sudah di-`.gitignore`, jadi tidak ikut ter-commit).

Tidak perlu `idf.py set-target` maupun `menuconfig`: semua config board (PSRAM octal, flash 16 MB, dan setting LVGL yang wajib) sudah ada di `sdkconfig.defaults`. `sdkconfig` sendiri **tidak** di-commit — kalau kamu mengubah `sdkconfig.defaults`, hapus `sdkconfig` lalu build ulang supaya perubahannya terbaca.

Cek cepat tanpa hardware sama sekali:

```bash
sh tools/run_selftests.sh   # codec RS485 + SVM + urutan power-off, ASan/UBSan
```

Flash dan log memakai port USB-C yang sama (USB native ESP32-S3). Bila gagal flash: tutup serial monitor, tahan **BOOT** saat menyalakan, flash, lalu power-cycle.

### Debug console

Aktifkan `CONFIG_TB_DEBUG_CONSOLE` untuk dapat REPL di `idf.py monitor`: suntik frame (`rfid`, `vital`, `status`, `btn`), scan bus I²C (`i2c`, `i2creg`, `i2craw`, `i2cdump`), dan `stats`. **Jangan aktif di unit produksi** — siapa pun via USB bisa memalsukan tanda vital. Detail: `docs/firmware-architecture.md` §Debug console.

### Power off

Tombol Power di UI benar-benar mematikan board: `ui_board_power_off()` menulis ke **SW6106 PMIC** di I²C `0x3c` (board V3.0 tidak punya `SYS_EN`). **Ini bekerja bahkan saat USB tersambung**, jadi tidak ada cara mengetesnya tanpa kehilangan board. Sebelum rail turun, `TB_CMD_POWER_OFF` dikirim ke STM32 supaya sensor dan LoRa sempat parkir. Urutan register: `docs/firmware-architecture.md` §Power off.

## Untuk kontributor baru

| Kamu mengerjakan | Baca ini | Sentuh ini |
| --- | --- | --- |
| **STM32** (sensor, tombol, RFID, LoRa) | `docs/firmware-architecture.md` §wire format | `components/triagebox_link/tb_frame.[ch]` — pakai apa adanya di sisi STM32, jangan bikin framing sendiri |
| **ML / SVM** | `docs/firmware-architecture.md` §SVM | `components/triagebox_ml/include/tb_svm_model.h` saja (bobot hasil training). `tb_svm.c` tidak perlu diubah |
| **UI / layar** | `docs/ui-workflow.md` | XML di `ui/screens/` lewat LVGL Pro Editor, lalu export. **Jangan** edit `*_gen.c` |

Aturan yang bikin repo ini tetap waras: `ui/logic/` platform-neutral (tanpa `ESP_PLATFORM`/SDL ifdef), `ui_action()` satu-satunya dispatcher tombol, dan `*_gen.c` selalu hasil export. Detail di `AGENTS.md`.

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
- `docs/firmware-architecture.md` — arsitektur firmware, wire format RS485, cara isi model SVM
- `docs/ui-workflow.md` — cara authoring layar di LVGL Pro Editor + export
- `docs/integration-esp32-stm32.md` — hanya untuk yang mengintegrasikan UI ke tree firmware **lain**
- `../triagebox-backend/docs/api-contract.md` — kontrak MQTT/REST/WS lengkap
