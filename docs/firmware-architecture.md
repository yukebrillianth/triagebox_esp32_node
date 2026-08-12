# Arsitektur firmware ESP32 TriageBox (node)

Repo ini berisi firmware ESP32-S3 lengkap untuk node TriageBox: **display + inference**. Semua sensor, 4 tombol fisik, RC522, dan LoRa SX1278 dipegang STM32.

```
                   RS485 (SP3485)
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

## Catatan hardware yang belum terverifikasi

`tb_link.c` pakai `UART_NUM_2` di GPIO44 (TX) / GPIO43 (RX). UART2, bukan UART0, karena GPIO43/44 adalah pin console default ESP32-S3 — console tetap di USB Serial/JTAG sehingga `idf.py monitor` masih jalan saat link aktif.

**TODO:** `README.md` menyebut SP3485 auto TX/RX switching, jadi mode UART biasa (tanpa pin DE) seharusnya benar. Cek skematik revisi board fisik sebelum menyolder. Kalau DE ternyata manual, ganti ke `UART_MODE_RS485_HALF_DUPLEX` + RTS — dan budget GPIO tidak punya kandidat bebas, jadi ada peripheral yang harus dikorbankan.

## Verifikasi

```sh
sh tools/run_selftests.sh          # semua selftest host, ASan+UBSan, tanpa hardware
source ~/.espressif/v6.0.2/esp-idf/export.sh && idf.py build
cmake -S sim -B /tmp/simcheck && cmake --build /tmp/simcheck -j8   # sim masih pakai mock
idf.py -p /dev/cu.usbmodem11301 flash monitor
```

Yang dicek di board: log `tb_link: uart2 up: tx=44 rx=43 @115200` dan `TriageBox UI up on 480x480`, tanpa panic. **Tanpa STM32 tersambung UI harus idle di Home**, bukan crash — itu memang kondisi yang diuji.

**Loopback tanpa STM32:** jumper GPIO43↔GPIO44, kirim `CMD`, lalu cek `tb_link_frames_ok()` naik. Membuktikan UART + codec benar sebelum sisi STM32 siap. Frame `CMD`/`RESULT` yang kembali sengaja diabaikan `dispatch()` (log level DEBUG).
