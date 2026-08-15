# Arsitektur firmware ESP32 TriageBox (node)

Repo ini berisi firmware ESP32-S3 lengkap untuk node TriageBox: **display + inference**. Semua sensor, 4 tombol fisik, RC522, dan LoRa SX1278 dipegang STM32.

```
                RS485 (MAX13487EESA+)
  STM32  ──────────────────────────────>  ESP32-S3  ──> LVGL 480×480
  (sensor,btn,      VITAL/BUTTON/RFID/       │
   RC522, LoRa)     STATUS                   │  SVM inference
         <──────────────────────────────────-┘
                   CMD / RESULT
  STM32 ──LoRa SX1278──> Station ──Ethernet──> Backend + Dashboard
```

LoRa **tidak** ada di ESP32: budget GPIO board ini habis (`AGENTS.md` → GPIO budget), dan STM32 sudah punya SPI untuk RC522. ESP32 mengirim hasil inference balik lewat frame `RESULT`, STM32 yang meneruskan ke station.

## Peta komponen

| Path | Isi |
| --- | --- |
| `main/` | bring-up saja: `app_main.c`, `asset_fs.c` |
| `ui/` | LVGL Pro project (XML + generated C) |
| `ui/logic/` | logic layer platform-neutral — **tidak** tahu soal RS485 maupun SVM |
| `components/esp32_s3_touch_lcd_4/` | BSP ter-vendor (patch IDF v6 + flip 180°) |
| `components/triagebox_link/` | RS485 ↔ STM32 |
| `components/triagebox_ml/` | inference SVM |
| `sim/` | simulator SDL desktop |
| `tools/run_selftests.sh` | jalankan semua selftest di host |

## Seam: satu header, dua implementasi

`ui/logic/ui_mock.h` adalah satu-satunya pintu masuk data ke UI. Implementasinya dipilih **di CMake, bukan `#ifdef`**:

| Target | File | Sumber data |
| --- | --- | --- |
| `sim/` | `ui/logic/ui_mock.c` | fake deterministik (QA desktop) |
| `main/` | `components/triagebox_link/tb_ui_source.c` | RS485 + SVM |

Akibatnya `ui/logic/` tidak berubah satu baris pun saat pindah dari mock ke hardware — dan sim tetap bisa dijalankan tanpa STM32. **Apa pun yang ditambahkan ke `ui_mock.h` wajib diimplementasikan di kedua file.**

Trigger inference sudah ada tanpa kode baru: `ui_runtime.c` memanggil `ui_mock_get_priority()` tepat sekali setelah measure selesai (`pull_mock_priority_once`), jadi `tb_ui_source.c` menjalankan SVM di situ lalu langsung mengirim `RESULT`.

## Wire format RS485

Little-endian. Framing eksplisit karena RS485 bisa kehilangan byte:

```
0xA5 0x5A | kind:u8 | len:u8 | payload[len] | crc16:u16
```

CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`) atas `kind+len+payload`.

| Kind | Arah | Payload |
| --- | --- | --- |
| `VITAL` 0x01 | STM32→ESP32 | `hr,spo2,rr,bp_sys,bp_dia:u16` + `battery:u8` + `flags:u8` |
| `BUTTON` 0x02 | STM32→ESP32 | `index:u8` (0..3) + `pressed:u8` |
| `RFID` 0x03 | STM32→ESP32 | `tag[len]` ASCII, ≤31, **tanpa NUL** |
| `STATUS` 0x04 | STM32→ESP32 | `sensor_ok:u8` bitmask + `battery:u8` |
| `CMD` 0x10 | ESP32→STM32 | `cmd:u8` — START_SCAN/START_MEASURE/ABORT/POWER_OFF |
| `RESULT` 0x11 | ESP32→STM32 | `priority:u8` + `confidence:u8` (0..100) + `tag[]` |

`flags` bit 0 = `TB_VITAL_FLAG_VALID`. Kalau 0, UI menampilkan `--` dan SVM menolak melakukan klasifikasi.

**Jebakan nomor satu:** `priority` di kabel pakai alias numerik LoRa `0=BLACK, 1=RED, 2=YELLOW, 3=GREEN`, sedangkan `ui_priority_t` urutannya `RED, YELLOW, GREEN, BLACK`. **Selalu** lewat `tb_frame_priority_to_wire()` / `_from_wire()`. Ada selftest khusus untuk ini.

`tb_frame.c` tidak punya malloc dan tidak butuh ESP-IDF — **developer STM32 bisa memakai file ini apa adanya** supaya kedua sisi tidak mungkin beda interpretasi.

### Batasan yang sudah diketahui

Frame yang terpotong di tengah payload akan **memakan frame berikutnya**: parser mengira sync bytes frame berikutnya adalah payload. Parser tetap resync, jadi frame ke-3 lolos — biayanya satu frame hilang. Ini konsekuensi length-prefix tanpa byte stuffing, bukan bug, dan ada selftest yang mendokumentasikannya.

Bisa diterima karena `VITAL`/`STATUS` datang berulang, dan `BUTTON` yang hilang = satu tekan yang operator ulangi. Kalau nanti ada frame yang tidak boleh hilang, tambahkan COBS stuffing supaya sync bytes tidak mungkin muncul di dalam payload.

## SVM

`components/triagebox_ml/` — linear one-vs-rest, 5 fitur (`hr, spo2, rr, bp_sys, bp_dia`), 4 kelas.

```c
ui_priority_t tb_svm_classify(const vitals_t *v, float *confidence);
```

- `z = (x - mean) / std` (StandardScaler), `score[c] = dot(w[c], z) + b[c]`, kelas menang = argmax.
- `confidence` = softmax atas skor, selalu di `[0,1]` seperti yang backend minta. Perlu dicatat: SVM one-vs-rest **tidak** menghasilkan probabilitas terkalibrasi — ini pemetaan skor→`[0,1]` yang monoton, bukan peluang sebenarnya. Nilai minimumnya 0.25 (semua skor sama).
- `v->valid == false` → `UI_PRIORITY_BLACK`, confidence 0. Menebak prioritas dari vital yang tidak ada lebih berbahaya daripada mengaku tidak tahu.
- Age band + gender **bukan** fitur model (proposal hanya menyebut variabel fisiologis). UI tetap mengumpulkannya untuk registrasi korban.
- `reasons` dikirim kosong: SVM tidak punya jalur aturan untuk dikutip, dan tidak dibuat tabel ambang terpisah. Backend memang men-default `reasons` ke `[]`.

### ⚠️ Model sekarang PLACEHOLDER

`include/tb_svm_model.h` isinya nol semua. Argmax selalu jatuh ke baris pertama, jadi **setiap** pasien dengan vital valid dikategorikan `RED` dengan confidence 0.25. Firmware link dan flow bisa didemo, tapi **belum melakukan triase.**

Cara mengganti dari notebook training (sklearn `LinearSVC` + `StandardScaler`):

| Konstanta | Dari |
| --- | --- |
| `K_MEAN` / `K_STD` | `scaler.mean_` / `scaler.scale_` |
| `K_W` (4×5) | `clf.coef_` |
| `K_B` (4) | `clf.intercept_` |

Urutan baris `K_W`/`K_B` **wajib** `RED, YELLOW, GREEN, BLACK` (urutan `ui_priority_t`), bukan urutan wire. Isi juga header komentarnya: tanggal training, ukuran dataset, akurasi, sensitivitas — target PKM akurasi >90%, sensitivitas >95%.

## Catatan hardware

`tb_link.c` pakai `UART_NUM_2` di GPIO44 (TX) / GPIO43 (RX). UART2, bukan UART0, karena GPIO43/44 adalah pin console default ESP32-S3 — console tetap di USB Serial/JTAG sehingga `idf.py monitor` masih jalan saat link aktif.

**Transceiver RS485 sudah terverifikasi dari skematik V3.0: `MAX13487EESA+` (U7), bukan SP3485** seperti yang lama tertulis di `README.md`. MAX13487E adalah varian *AutoDirection* — arah TX/RX diatur di dalam chip, **tidak ada pin DE/RE**. Jadi mode UART biasa memang benar: `UART_MODE_RS485_HALF_DUPLEX` + RTS tidak diperlukan, dan tidak ada peripheral yang perlu dikorbankan untuk pin DE. U7 jalan di 5 V dengan level shifter transistor diskrit ke net 3V3 `485_TXD`/`485_RXD`; terminasi 120 Ω dipilih lewat `SW1`, dipakai bersama CAN.

### Power off (SW6106)

Board V3.0 **tidak punya `SYS_EN`**. Baterai dan rail 5 V/3V3 dipegang **SW6106 PMIC di I²C `0x3c`** (pin `LED4/I2C` di-strap ke GND lewat R8 0R, jadi chip berada di mode I²C). Pin `KEY`-nya hanya ke tactile switch, tanpa net MCU — jadi satu-satunya cara firmware mematikan dirinya sendiri adalah lewat register.

`ui_board_power_off()` (`components/triagebox_board/ui_board.c`) menjalankan urutan dari **SW6106 I2C Register List RG006_1_v1.2**:

1. Baca `REG 0x49`, batalkan kalau bit 3 (*key control output power off enable*) tidak set.
2. Write-unlock: `REG 0x01` ← `0x40` (bit 7:6 = 1), lalu `REG 0x01` ← `0x80` (= 2).
3. `REG 0x03` ← `0x10` (bit 4 = *output power off*, self-clearing).

> **Terverifikasi di hardware: ini benar-benar mematikan board, bahkan saat USB tersambung.** Tidak ada mode dry-run — sekali dipanggil, board mati. Simpan dulu apa pun yang harus selamat.

`ui_mock_power_off()` mengirim `TB_CMD_POWER_OFF` ke STM32 dulu, tunggu 150 ms, baru cut rail — STM32 tidak memegang rail tapi memegang sensor dan LoRa yang menempel di rail itu, jadi ia butuh waktu untuk parkir. Frame-nya fire-and-forget: STM32 yang tidak ada tidak boleh menghalangi shutdown.

Urutan write ini dipatok di `components/triagebox_board/ui_board_power_selftest.c`, yang meng-compile `ui_board.c` asli di host lewat `components/triagebox_board/test_fakes/`.

## Verifikasi

```sh
sh tools/run_selftests.sh          # semua selftest host, ASan+UBSan, tanpa hardware
source ~/.espressif/v6.0.2/esp-idf/export.sh && idf.py build
cmake -S sim -B /tmp/simcheck && cmake --build /tmp/simcheck -j8   # sim masih pakai mock
idf.py flash monitor               # port auto-detect; -p COM7 / -p /dev/cu.usbmodem* kalau perlu
```

Yang dicek di board: log `tb_link: uart2 up: tx=44 rx=43 @115200` dan `TriageBox UI up on 480x480`, tanpa panic. **Tanpa STM32 tersambung UI harus idle di Home**, bukan crash — itu memang kondisi yang diuji.

**Loopback tanpa STM32:** jumper GPIO43↔GPIO44, kirim `CMD`, lalu cek `tb_link_frames_ok()` naik. Membuktikan UART + codec benar sebelum sisi STM32 siap. Frame `CMD`/`RESULT` yang kembali sengaja diabaikan `dispatch()` (log level DEBUG).

## Debug console

`CONFIG_TB_DEBUG_CONSOLE` (menuconfig → *TriageBox debug*) menambah REPL di USB Serial/JTAG. **Jangan aktif di unit produksi** — siapa pun yang tersambung USB bisa memalsukan tanda vital. REPL start paling akhir di `app_main()` supaya frame yang disuntik jatuh ke layar yang sudah hidup, dan supaya bus I²C sudah dibawa naik BSP.

| Command | Fungsi |
| --- | --- |
| `rfid [tag]` | Suntik frame RFID |
| `vital [hr spo2 rr sys dia]` | Suntik VITAL |
| `status [mask] [lora_ok]` | Suntik STATUS |
| `btn 0..3` | Tekan tombol fisik |
| `i2c [timeout_ms]` | Scan bus `0x08..0x77` + nama device yang dikenal |
| `i2creg <addr> <reg> [count] [split]` | Baca register |
| `i2craw <addr> [count]` | Baca tanpa write pointer register |
| `i2cdump <addr> [start] [end]` | Dump ruang register, hanya yang non-zero yang ditandai |
| `stats` | CPU/heap/stack + `frames_ok`/`crc_errors` |

Semua perintah I²C **read-only** — tidak ada `i2cwrite`. Alamat `0x3c` adalah charger 4 A dengan LiPo menempel; write yang salah bisa mengubah tegangan cut-off atau mematikan power path. Satu-satunya write ke PMIC ada di `ui_board_power_off()`, urutannya dari datasheet dan dipatok selftest.

Catatan pemakaian: pointer register **tidak auto-increment** di TCA9554 maupun SW6106 — `count > 1` membaca register yang sama berulang, jadi baca satu-satu. Byte `ff` setelah byte pertama biasanya bus floating, bukan data.
