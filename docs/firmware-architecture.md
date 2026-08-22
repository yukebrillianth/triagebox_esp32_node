# Arsitektur firmware ESP32 TriageBox (node)

Repo ini berisi firmware ESP32-S3 lengkap untuk node TriageBox: **display + inference**. Semua sensor, 4 tombol fisik, RC522, dan LoRa SX1278 dipegang STM32.

```
                I²C (bus display, 0x42)
  STM32  <─────────────────────────────>  ESP32-S3  ──> LVGL 480×480
  (sensor,btn,      snapshot 50 ms /         │
   PN532, LoRa)     CMD + RESULT             │  SVM inference
  STM32 ──LoRa SX1278──> Station ──Ethernet──> Backend + Dashboard
```

LoRa **tidak** ada di ESP32: budget GPIO board ini habis (`AGENTS.md` → GPIO budget), dan STM32 sudah punya SPI untuk PN532. ESP32 mengirim hasil inference balik lewat register `RESULT`, STM32 yang meneruskan ke station.

## Peta komponen

| Path | Isi |
| --- | --- |
| `main/` | bring-up saja: `app_main.c`, `asset_fs.c` |
| `ui/` | LVGL Pro project (XML + generated C) |
| `ui/logic/` | logic layer platform-neutral — **tidak** tahu soal I²C maupun SVM |
| `components/esp32_s3_touch_lcd_4/` | BSP ter-vendor (patch IDF v6 + flip 180° + read touch non-fatal) |
| `components/triagebox_link/` | link I²C ↔ STM32 (+ `tb_frame.c` untuk payload LoRa) |
| `components/triagebox_ml/` | inference SVM |
| `sim/` | simulator SDL desktop |
| `tools/run_selftests.sh` | jalankan semua selftest di host |

## Seam: satu header, dua implementasi

`ui/logic/ui_mock.h` adalah satu-satunya pintu masuk data ke UI. Implementasinya dipilih **di CMake, bukan `#ifdef`**:

| Target | File | Sumber data |
| --- | --- | --- |
| `sim/` | `ui/logic/ui_mock.c` | fake deterministik (QA desktop) |
| `main/` | `components/triagebox_link/tb_ui_source.c` | I²C + SVM |

Akibatnya `ui/logic/` tidak berubah satu baris pun saat pindah dari mock ke hardware — dan sim tetap bisa dijalankan tanpa STM32. **Apa pun yang ditambahkan ke `ui_mock.h` wajib diimplementasikan di kedua file.**

Trigger inference sudah ada tanpa kode baru: `ui_runtime.c` memanggil `ui_mock_get_priority()` tepat sekali setelah measure selesai (`pull_mock_priority_once`), jadi `tb_ui_source.c` menjalankan SVM di situ lalu langsung mengirim `RESULT`.

## Wire ESP32 ↔ STM32: register map I²C

**Sumber kebenaran tunggal: `components/triagebox_link/include/tb_regs.h`**, salinan verbatim dari file yang sama di project STM32. Offset **tidak** diduplikasi di dokumen ini — kalau ada dua daftar, salah satunya akan basi. Edit satu, copy ke yang lain; `TB_PROTO_VER` di reg `0x00` dibaca lebih dulu supaya salinan basi gagal berisik, bukan salah baca offset.

ESP32-S3 = master, STM32F411 = slave `0x42`, di **bus I²C display yang sudah ada** (SDA GPIO15 / SCL GPIO7). Tidak ada pin tambahan, tidak ada transceiver.

```
read : S 0x42 W [reg] Sr 0x42 R [d0] [d1] ... P     (snapshot, pointer auto-increment)
write: S 0x42 W [reg] [d0] [d1] ... P
```

Slave melatch salinan konsisten saat master mengalamatinya untuk read, jadi satu read multi-byte tidak mungkin mencampur HR lama dengan SpO2 baru. Little-endian. Poll 50 ms dari timer LVGL.

Kenapa register map, bukan `tb_frame`: I²C sudah memberi apa yang dulu dibangun sync bytes + CRC di RS485 — start/stop membatasi tiap transaksi dan tiap byte di-ACK hardware. Untungnya di debuggability: STM32 terlihat seperti chip I²C biasa, jadi `i2creg` / `i2cdump` / `i2craw` yang sudah ada bisa mengintipnya tanpa tooling baru.

Dua hal yang tidak terlihat dari file header:

- **`TB_REG_BUTTONS` adalah state mask, bukan event.** `tb_i2c_codec.c` yang men-diff jadi edge press/release. Satu poll bisa sah menghasilkan sampai 4 edge sekaligus (dua jari, atau satu poll terlewat saat task LVGL sibuk), jadi antrian tombol di `tb_ui_source.c` panjangnya 8 — bukan satu slot.
- **`rfid_len == 0` adalah informasi, bukan diam.** Itu satu-satunya bukti STM32 sudah memproses `START_SCAN` dan melepas kartu pasien sebelumnya. Lihat §"Gate RFID".

### RS485 sudah disuperseded

`tb_link.c` (UART2 GPIO44/43, framing `0xA5 0x5A` + CRC-16/CCITT-FALSE) disimpan satu rilis kalau swap-nya harus di-revert. STM32 project tidak pernah punya USART, jadi I²C adalah satu-satunya transport yang pernah punya dua ujung. `tb_frame.c` tetap tinggal: payload LoRa masih memakai konversi prioritasnya, dan file itu host-tested.

**Jebakan nomor satu (masih berlaku):** `priority` di kabel pakai alias numerik LoRa `0=BLACK, 1=RED, 2=YELLOW, 3=GREEN`, sedangkan `ui_priority_t` urutannya `RED, YELLOW, GREEN, BLACK`. **Selalu** lewat `tb_frame_priority_to_wire()` / `_from_wire()`. Ada selftest khusus untuk ini.

`tb_frame.c` dan `tb_i2c_codec.c` dua-duanya tanpa malloc dan tanpa ESP-IDF — **developer STM32 bisa memakai file ini apa adanya** supaya kedua sisi tidak mungkin beda interpretasi.

## Status bar dan gate RFID

Keduanya di `ui/logic/ui_bindings.c`, dan keduanya menimpa literal yang di-hardcode `*_gen.c` (`"80%"`, `"Connected"`, `"--:--"`).

`sync_status_bar()` jalan di **semua** layar (beda dari `ui_bindings_sync_status_dots()` yang early-return di luar Home), cadence 1 s, dengan PMIC dibaca ulang tiap 10 s. Nama widget: `sb_battery`, `sb_battery_text`, `sb_link_text`, `sb_clock`.

- Persen baterai dari SW6106 lewat `ui_board_battery()` — read-only, reg `0x4F[6:0]` + `0x11[4]` untuk charging. Read gagal → `UNKNOWN` (`--%`), bukan nilai bagus terakhir: 80% yang beku sementara pack habis lebih berbahaya daripada mengaku tidak tahu.
- Status LoRa dari bit `sensor_ok` STM32. Glyph sinyal di sebelahnya **tanpa nama** di `status_bar_gen.c`, jadi state ditunjukkan lewat **warna teks** — tidak perlu regenerate XML.
- Jam tetap `--:--` sampai PCF85063A (`0x51`) dibaca; belum ada RTC battery, jadi jalur `settime` menyusul.

### Gate RFID

Kalau operator menekan Restart lalu mulai scan, snapshot berikutnya masih membawa kartu pasien **sebelumnya**: STM32 memang menghapus `rfid_ascii_len` saat melayani `START_SCAN`, tapi itu terjadi hingga satu superloop (~10–20 ms) kemudian, sementara ESP32 poll tiap 50 ms. Layar scanning menerimanya seketika.

Perbaikannya dua bagian, dua-duanya di sisi ESP32: `tb_link_i2c.c` mem-push `rfid_t` kosong saat `rfid_len == 0`, dan `tb_ui_source.c` memasang `s_rfid_gate` yang di-arm oleh `ui_mock_start_scan()` dan menolak semua tag sampai snapshot kosong itu datang. **Gate tetap dipasang walau write `START_SCAN` gagal** — scan yang tidak pernah selesai adalah kerusakan yang kelihatan, scan yang selesai dengan identitas salah tidak.

Satu invariant yang mudah dilanggar: snapshot kosong saat gate **terbuka** tidak boleh menghapus `s_rfid`. Monitor dan Result membaca tag itu sampai sesi berakhir. Dipatok di `components/triagebox_link/tb_ui_source_selftest.c`, yang meng-compile file device asli di host lewat `test_fakes/`.

Bunyi buzzer sekali saat scan sukses di-arm oleh layar **SCANNING**, bukan oleh "Berhasil sedang tampil": Berhasil juga bisa dicapai mundur dari Age, dan bip di situ berarti "kartu terbaca" padahal tidak ada yang dibaca.

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

Link STM32 memakai bus I²C yang sudah dibawa naik BSP — `tb_link_start()` **wajib** dipanggil setelah `bsp_display_start()`, kalau tidak `bsp_i2c_get_handle()` belum ada dan link menolak start dengan log yang jelas.

Sisa RS485 (relevan hanya kalau swap di-revert): `tb_link.c` pakai `UART_NUM_2` di GPIO44 (TX) / GPIO43 (RX). UART2, bukan UART0, karena GPIO43/44 adalah pin console default ESP32-S3 — console tetap di USB Serial/JTAG sehingga `idf.py monitor` masih jalan saat link aktif. Transceiver-nya `MAX13487EESA+` (U7) varian *AutoDirection*: arah TX/RX diatur di dalam chip, **tidak ada pin DE/RE**, jadi UART biasa memang benar dan tidak ada peripheral yang perlu dikorbankan. Terminasi 120 Ω dipilih lewat `SW1`, dipakai bersama CAN.

### Power off (SW6106)

Board V3.0 **tidak punya `SYS_EN`**. Baterai dan rail 5 V/3V3 dipegang **SW6106 PMIC di I²C `0x3c`** (pin `LED4/I2C` di-strap ke GND lewat R8 0R, jadi chip berada di mode I²C). Pin `KEY`-nya hanya ke tactile switch, tanpa net MCU — jadi satu-satunya cara firmware mematikan dirinya sendiri adalah lewat register.

`ui_board_power_off()` (`components/triagebox_board/ui_board.c`) menjalankan urutan dari **SW6106 I2C Register List RG006_1_v1.2**:

1. Baca `REG 0x49`, batalkan kalau bit 3 (*key control output power off enable*) tidak set.
2. Write-unlock: `REG 0x01` ← `0x40` (bit 7:6 = 1), lalu `REG 0x01` ← `0x80` (= 2).
3. `REG 0x03` ← `0x10` (bit 4 = *output power off*, self-clearing).

> **Terverifikasi di hardware: ini benar-benar mematikan board, bahkan saat USB tersambung.** Tidak ada mode dry-run — sekali dipanggil, board mati. Simpan dulu apa pun yang harus selamat.

`ui_mock_power_off()` mengirim `TB_CMD_POWER_OFF` ke STM32 dulu, tunggu 150 ms, baru cut rail — STM32 tidak memegang rail tapi memegang sensor dan LoRa yang menempel di rail itu, jadi ia butuh waktu untuk parkir. Frame-nya fire-and-forget: STM32 yang tidak ada tidak boleh menghalangi shutdown.

Urutan write ini dipatok di `components/triagebox_board/ui_board_power_selftest.c`, yang meng-compile `ui_board.c` asli di host lewat `components/triagebox_board/test_fakes/`.

## Blackscreen: penyebab sudah dipastikan

Gejala yang dilaporkan: layar tiba-tiba hitam **tapi backlight tetap menyala**, "seperti reset". Ternyata memang **panic reboot**, dan pemicunya satu transaksi I²C GT911 yang gagal:

```
E (3376122) i2c.master: I2C transaction timeout detected
E (3376132) lcd_panel.io.i2c: panel_io_i2c_rx_buffer(145): i2c transaction failed
E (3376132) GT911: esp_lcd_touch_gt911_read_data(232): I2C read error!
ESP_ERROR_CHECK failed: esp_err_t 0x108 (ESP_ERR_INVALID_RESPONSE)
--- lvgl_port_touchpad_read at esp_lvgl_port_touch.c:127
abort() was called at PC 0x4037dc53 on core 0
```

`esp_lvgl_port` membungkus `esp_lcd_touch_read_data()` dengan `ESP_ERROR_CHECK` (`esp_lvgl_port_touch.c:127`), jadi **satu** read touch yang gagal memanggil `abort()` → panic → reboot. Heartbeat 3,5 s sebelumnya masih sehat (`heap=5769512 min=5765812 frames=56262`, uptime ~56 menit), yang menyingkirkan heap, RGB desync, dan task macet.

Kenapa gejalanya seperti panel mati: backlight ada di EXIO2 TCA9554 yang **tidak** ikut reset bersama SoC, dan `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT` + `REBOOT_DELAY_SECONDS=0` + coredump off membuat panic reboot seketika tanpa jejak di layar.

Ini juga menjelaskan timeout GT911 di ~84 s dan ~157 s yang sebelumnya tidak jelas: keduanya kena `panel_io_i2c_tx_buffer(193)`, yaitu write "clear status" di `esp_lcd_touch_gt911.c:236` yang **nilai kembaliannya dibuang**, jadi read tetap `ESP_OK` dan tidak ada yang abort. Fault yang sama, hanya beda transaksi mana yang kena.

**Perbaikan** (`components/esp32_s3_touch_lcd_4/esp32_s3_touch_lcd_4.c`, `touch_read_tolerant()`): BSP memasang read callback sendiri lewat `lv_indev_set_read_cb()` sesudah `lvgl_port_add_touch()`, membaca touch tanpa `ESP_ERROR_CHECK`, dan **menahan state terakhir** kalau read gagal (bukan memaksa RELEASED — release palsu di tengah tekanan asli = klik hantu, dan di layar ini klik memulai/membatalkan pengukuran). Errornya di-log WARN dengan counter supaya glitch bus tetap kelihatan sekarang setelah tidak lagi bikin panic. Scale port-nya 1:1 di board ini, jadi tidak ada yang hilang.

Yang **belum** diperbaiki: penyebab timeout-nya sendiri. Bus itu dipakai bareng TCA9554, SW6106, PCF85063A, GT911, dan STM32 (poll 50 ms) **tanpa lock lintas komponen dan tanpa recovery SDA yang nyangkut**. Sekarang efeknya turun dari reboot jadi satu sample touch hilang, jadi urutannya benar: hilangkan yang fatal dulu, kurangi kontensinya nanti dengan data dari counter itu. Kandidat kalau counternya ternyata tinggi: retry di `touch_read_tolerant()`, perlebar interval poll STM32, atau satu mutex bus di level BSP.

Dua kandidat lain **sudah dieliminasi lewat baca kode**, bukan lewat dugaan, dan tetap dicatat supaya tidak dikejar lagi: (1) TCA9554 re-init yang membuat `LCD_RST` floating — mustahil, `BSP_LCD_RST` = `GPIO_NUM_NC` (panel ini tidak punya jalur reset yang digerakkan) dan jalur display memakai `.io_expander = NULL`; (2) read SPIFFS runtime untuk font/gambar yang mematikan cache dan membuat bounce buffer RGB underrun — gambar adalah array C yang di-compile, dan `asset_fs.c` sudah menyalin tiap font ke blob PSRAM sekali lalu melayani semua read/seek dari RAM.

### Instrumen diagnosa

Dua instrumen di `main/app_main.c`. Keduanya tetap dipakai: yang menangkap kasus di atas adalah heartbeat sehat tepat sebelum panic, dan gejala "layar hitam" berikutnya (kalau ada) harus dibedakan dari yang ini.

| Log | Kapan | Isi |
| --- | --- | --- |
| `boot: reset=<nama> (<n>), heap free=.. min=..` | statement pertama `app_main()` | menamai PANIC / BROWNOUT / int-wdt / task-wdt |
| `hb: lv=.. wall=.. screen=.. heap=.. min=.. frames=..` | tiap 200 tick timer LVGL (10 s) | uptime, layar aktif, heap, `frames_ok` |

Heartbeat sengaja ditaruh **di dalam callback timer LVGL**: kalau baris itu tercetak, task LVGL hidup dan timer-nya jalan. Level WARN karena `CONFIG_LV_LOG_LEVEL_WARN` akan menelan `LV_LOG_USER`.

Cara membacanya saat kejadian:

| Yang terlihat di log | Artinya | Langkah berikut |
| --- | --- | --- |
| uptime mulai lagi dari ~0 | memang reset | baca baris `boot: reset=` di atasnya; kalau PANIC, cari `abort()`/backtrace di atasnya |
| uptime terus naik saat layar hitam | SoC + UI sehat, stream panel mati | coba `CONFIG_LCD_RGB_RESTART_IN_VSYNC` (1 fb PSRAM + bounce buffer 480×20; **belum diverifikasi ke dokumen IDF v6.0.2**) |
| log berhenti, tanpa baris boot | task LVGL macet | tersangka utama bus I²C bersama |
| `lv=` dan `wall=` melebar jaraknya | tick source LVGL kelaparan, SoC sehat | di panel kelihatan sama, perbaikannya beda |
| `touch read failed (..), errors=N` naik cepat | kontensi bus I²C sering | sebelumnya ini yang bikin reboot; sekarang cuma sample hilang |

Tes tanpa kode yang menyertainya: **saat layar hitam, tekan tombol.** Kalau masih ada bip, SoC hidup — berarti jalur display, bukan reset.

Riwayat: run 190 s pertama sesudah instrumen masuk tidak blackout sama sekali (heartbeat mulus, heap datar ~5,68 MB, `lv`/`wall` selisih konstan 132 ms = offset boot, bukan starvation). Blackout baru tertangkap di run ~56 menit, dan itu yang jadi bukti di atas.

## Verifikasi

```sh
sh tools/run_selftests.sh          # semua selftest host, ASan+UBSan, tanpa hardware
source ~/.espressif/v6.0.2/esp-idf/export.sh && idf.py build
cmake -S sim -B /tmp/simcheck && cmake --build /tmp/simcheck -j8   # sim masih pakai mock
idf.py flash monitor               # port auto-detect; -p COM7 / -p /dev/cu.usbmodem* kalau perlu
```

Yang dicek di board: log `tb_link: i2c link up: addr=0x42 poll=50ms` diikuti `STM32 found at 0x42` dan `TriageBox UI up on 480x480`, tanpa panic. **Tanpa STM32 tersambung UI harus idle di Home**, bukan crash — itu memang kondisi yang diuji; link akan bilang `no answer from 0x42` dan tetap poll.

Kalau STM32 menjawab tapi `seq` tidak berubah, link mencetak `seq frozen` — itu superloop STM32 yang berhenti publish, bukan bus yang rusak. Salinan `tb_regs.h` yang basi ketahuan lewat warning `proto_ver mismatch`.

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
| `i2clink` | Baca + decode snapshot STM32 di `0x42` (proto ver, seq, vital, tombol, RFID) |
| `pmic [samples] [period_ms]` | Telemetri daya SW6106: SoC, Vbat, Vout, Ichg, **Idischg**, Tdie |
| `stats` | CPU/heap/stack + `frames_ok`/`crc_errors` |

`pmic` menjawab "berapa arus yang dipakai sekarang": **`Idischg` adalah arus sisi baterai**, jadi mencakup ESP32 + LCD + STM32 sekaligus karena semuanya lewat satu jalur itu. Sumbernya blok ADC di *SW6106 I2C Register List* RG006_1_v1.2 §2.15–2.22 — `Vbat = ((0x15[3:0]<<8)|0x14) * 1.2 mV`, `Vout = ((0x15[7:4]<<8)|0x16) * 4 mV`, `Ichg = ((0x18[3:0]<<8)|0x17) * 25/7 mA`, `Idischg = ((0x18[7:4]<<8)|0x19) * 25/7 mA`, `Tdie = (((0x1B[3:0]<<8)|0x1A) − 1851)/6.82 °C`. Charge dan discharge **saling eksklusif**, kolom state (dari `0x11`) menunjukkan mana yang berlaku. Karena ini sisi baterai, arus di rail 5 V kira-kira `Idischg × Vbat/5 V` dikurangi rugi boost — jangan dibandingkan langsung dengan pengukuran di 5 V. Beri `samples`/`period_ms` untuk melihat serinya sambil menyalakan beban (backlight, LoRa TX).

Semua perintah I²C **read-only** — tidak ada `i2cwrite`. Alamat `0x3c` adalah charger 4 A dengan LiPo menempel; write yang salah bisa mengubah tegangan cut-off atau mematikan power path. Satu-satunya write ke PMIC ada di `ui_board_power_off()`, urutannya dari datasheet dan dipatok selftest.

Catatan pemakaian: pointer register **tidak auto-increment** di TCA9554 maupun SW6106 — `count > 1` membaca register yang sama berulang, jadi baca satu-satu. Byte `ff` setelah byte pertama biasanya bus floating, bukan data.
