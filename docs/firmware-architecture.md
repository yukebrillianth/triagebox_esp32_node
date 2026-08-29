# Arsitektur firmware ESP32 TriageBox (node)

Repo ini berisi firmware ESP32-S3 lengkap untuk node TriageBox: **display + inference**. Semua sensor, 4 tombol fisik, RC522, dan LoRa SX1278 dipegang STM32.

```
                I²C (bus display, 0x42)
  STM32  <─────────────────────────────>  ESP32-S3  ──> LVGL 480×480
  (sensor,btn,      snapshot 50 ms /         │
   PN532, LoRa)     CMD + RESULT             │  GBM inference
  STM32 ──LoRa SX1278──> Station ──Ethernet──> Backend + Dashboard
```

LoRa **tidak** ada di ESP32: budget GPIO board ini habis (`AGENTS.md` → GPIO budget), dan STM32 sudah punya SPI untuk PN532. ESP32 mengirim hasil inference balik lewat register `RESULT`, STM32 yang meneruskan ke station.

## Peta komponen

| Path | Isi |
| --- | --- |
| `main/` | bring-up saja: `app_main.c`, `asset_fs.c` |
| `ui/` | LVGL Pro project (XML + generated C) |
| `ui/logic/` | logic layer platform-neutral — **tidak** tahu soal I²C maupun model |
| `components/esp32_s3_touch_lcd_4/` | BSP ter-vendor (patch IDF v6 + flip 180° + read touch non-fatal) |
| `components/triagebox_link/` | link I²C ↔ STM32 (+ `tb_frame.c` untuk payload LoRa) |
| `components/triagebox_ml/` | inference GBM (ESI 1..5) + adapter ke warna START |
| `sim/` | simulator SDL desktop |
| `tools/run_selftests.sh` | jalankan semua selftest di host |

## Seam: satu header, dua implementasi

`ui/logic/ui_mock.h` adalah satu-satunya pintu masuk data ke UI. Implementasinya dipilih **di CMake, bukan `#ifdef`**:

| Target | File | Sumber data |
| --- | --- | --- |
| `sim/` | `ui/logic/ui_mock.c` | fake deterministik (QA desktop) |
| `main/` | `components/triagebox_link/tb_ui_source.c` | I²C + model |

Akibatnya `ui/logic/` tidak berubah satu baris pun saat pindah dari mock ke hardware — dan sim tetap bisa dijalankan tanpa STM32. **Apa pun yang ditambahkan ke `ui_mock.h` wajib diimplementasikan di kedua file.**

Trigger inference sudah ada tanpa kode baru: `ui_runtime.c` memanggil `ui_mock_get_priority()` tepat sekali setelah measure selesai (`pull_mock_priority_once`), jadi `tb_ui_source.c` menjalankan model di situ lalu langsung mengirim `RESULT`.

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

### RSSI: satu register di luar blok vital

`TB_REG_LORA_RSSI` (`0x30`) berisi **seberapa kuat node ini mendengar poll terakhir dari station**, dalam dBm. Ini satu-satunya angka kualitas link yang bisa diketahui node: node tidak pernah transmit tanpa dipoll, jadi radionya duduk di RX continuous dan `RegPktRssiValue` sesudah RxDone adalah pengukuran nyata **di posisi node**. Arahnya station→node, tapi path loss itu resiprokal, jadi untuk mengukur jarak dengan cara menjauhkan alat dari station inilah angka yang tepat.

RSSI arah sebaliknya (yang station dengar) diukur radio station sendiri dan masuk ke node status JSON-nya — tidak bisa sampai ke layar ini tanpa menambah field downlink, dan isinya akan mengatakan hal yang sama.

**Sengaja di luar `TB_REG_READ_END`, dan itu yang membuat `TB_PROTO_VER` tidak perlu naik.** Poll vital 50 ms tetap membaca tepat `0x30` byte, jadi STM32 yang dibangun sebelum register ini ada menjawab poll itu **tanpa perubahan apa pun** — dua board bisa di-flash terpisah, urutan bebas. ESP32 mengambil byte ini di transaksi 1-byte sendiri, 1 Hz (station poll tiap 15 s, jadi 20 Hz cuma membaca ulang byte yang tidak mungkin berubah, di bus yang dipakai bareng GT911). STM32 lama menjawab read itu dengan pad `0xFF`-nya, dan `tb_rssi_valid()` menolaknya.

Kalau ditaruh **di dalam** blok: master akan meminta satu byte lebih banyak daripada isi buffer STM32 lama → poll vital gagal → link terlihat mati. Itu harga yang jauh lebih besar daripada satu byte per detik.

Dua nilai berarti "belum ada bacaan", dari dua tempat berbeda — karena itu validasinya rentang, bukan `!= SENTINEL`: `0x00` = STM32 baru yang belum mendengar poll (0 dBm bukan level yang dilaporkan receiver mana pun), `0xFF` = STM32 lama atau alamat yang tidak dikenalinya. Keduanya di atas sinyal terkuat yang nyata, jadi batas atas saja sudah menolak keduanya. Batas bawah `-128` adalah `INT8_MIN` — tipenya sendiri yang jadi batas, dan menuliskan perbandingannya justru ditolak `-Werror=type-limits`.

Di layar: label sebelah ikon sinyal. Presedensi, paling actionable dulu — tidak ada STM32 (`Link --`) → radio mati (`LoRa mati`) → dBm terukur (`-97dBm`) → radio siap tapi belum dengar apa pun (`LoRa siap`). Warnanya mengikuti **margin link** begitu ada angka (`≥ -100` hijau, `≥ -115` kuning, di bawahnya merah; sensitivitas SX1278 di SF7/125k sekitar -123 dBm), bukan cuma "radio nyala" — jadi warnanya berubah **sebelum** link mati, yang tidak pernah dilakukan boolean. Tidak ada pemetaan ke bar sinyal: angkanya justru intinya, dan membaginya jadi 4 bar membuang resolusi yang dibutuhkan pengukuran jarak.

Read yang gagal **tidak** menghapus angka terakhir, berbeda dari baterai. Baterai adalah angka keselamatan yang tidak boleh terlihat lebih baik dari kenyataan; RSSI dibaca orang yang sedang berjalan sambil melihat layar, dan mengosongkannya karena satu transaksi hilang membuat angkanya tidak terbaca justru saat dipakai. Link yang benar-benar mati tetap mengosongkannya lewat jalur dot status.

#### Yang perlu dikerjakan di sisi STM32 🙏

Sisi ESP32 sudah lengkap dan aman dijalankan sekarang — tanpa perubahan di bawah, layar menampilkan `LoRa siap` dan `i2clink` mencetak `lora_rssi : -- (raw 0xff: STM32 predates this register...)`. Tiga langkah:

**1. Copy `tb_regs.h`** dari repo ini (yang berubah: `int8_t lora_rssi` di akhir `tb_snapshot_t`, plus `TB_REG_LORA_RSSI` / `TB_REG_SNAPSHOT_END` / `tb_rssi_valid()`). `TB_PROTO_VER` **tidak** naik, jadi urutan flash bebas dan tidak perlu serentak. `s_tx[sizeof(tb_snapshot_t)]` di `tb_slave.c` otomatis jadi 0x31 byte, dan itulah yang membuat register `0x30` bisa dibaca — `AddrCallback` sudah menangani `start < sizeof(s_tx)` tanpa perubahan.

`tools/tb_link_selftest.c` di repo STM32 **akan gagal** begitu header-nya di-copy, dan itu memang benar — dua assert-nya menyatakan struct berakhir tepat di `TB_REG_READ_END`:

```c
assert(sizeof(tb_snapshot_t) == TB_REG_READ_END);   /* jadi TB_REG_SNAPSHOT_END */
assert(sizeof(tb_snapshot_t) == 0x30U);             /* jadi 0x31U              */
```

Ganti keduanya ke `TB_REG_SNAPSHOT_END` / `0x31U` dan tambahkan `assert(offsetof(tb_snapshot_t, lora_rssi) == TB_REG_LORA_RSSI);`. Sisi ESP32 sudah memasang assert yang setara di `tb_i2c_codec_selftest.c`.

**2. Setter kecil di `tb_slave.c`**, bukan parameter baru di `tb_slave_publish()`: RSSI datang tiap poll (15 s) sedangkan publish jauh lebih sering, jadi parameter akan memaksa pemanggil menyimpan nilai terakhirnya sendiri.

```c
void tb_slave_set_rssi(int8_t dbm)
{
    s_stage.lora_rssi = dbm; /* publish berikutnya yang menyalinnya ke s_live */
}
```

**3. Di `main.c`, sesudah `lora_poll_for_me()` lolos** (paket itu memang dari station, jadi RSSI-nya berarti):

```c
int rssi = LoRa_getRSSI(&hlora);

/* WAJIB di-clamp. LoRa_getRSSI() mengembalikan -164 + RegPktRssiValue, jadi
 * rentangnya -164..+91 -- dan -164 sebagai int8_t membungkus jadi +92, yang
 * lolos setiap uji "masuk akal" dan tampil sebagai sinyal kuat. */
if (rssi < -128) { rssi = -128; }
if (rssi > 127)  { rssi = 127; }
mon_lora_rssi = (int16_t)rssi;      /* untuk CubeMonitor, tanpa clamp kalau mau */
tb_slave_set_rssi((int8_t)rssi);
```

Letaknya sesudah `++mon_lora_polls` dan **sebelum** `LoRa_transmit()`: `RegPktRssiValue` itu per-paket, dan transmit lalu kembali ke RX bisa menimpanya dengan paket node lain di kanal yang sama.

Tidak perlu mengirim RSSI lewat LoRa ke station — station mengukur arahnya sendiri. Ini murni untuk layar node.

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

## Model triase

`components/triagebox_ml/` — GBM (LightGBM, di-emit sebagai C) dengan **6 fitur**: `age, sex, systolic_bp, heart_rate, respiratory_rate, spo2`. Output mentahnya **ESI 1..5** plus vektor probabilitas 5 kelas.

```c
/* tb_triage.h -- satu-satunya pintu masuk dari UI */
ui_priority_t tb_triage_classify(const vitals_t *v, ui_age_band_t age,
                                 ui_gender_t gender, float *confidence,
                                 int *esi);
```

Modelnya makan **satu bacaan sesaat**, bukan agregat. Ini berubah dari pipeline sebelumnya yang minta mean/min/max sepanjang window — `tb_vitals_window_t` cuma ada untuk itu dan ikut hilang bersamanya. `infer_once()` sekarang mengirim snapshot terakhir apa adanya.

### ESI → warna: 3 kelas, standar START Indonesia

| ESI | Warna |
| --- | --- |
| 1 | 🔴 **MERAH** |
| 2 | 🟡 **KUNING** |
| 3, 4, 5 | 🟢 **HIJAU** |
| fitur ada yang 0 | ⚫ **HITAM** — menolak menilai |

Pemetaan ini milik sisi ML dan tinggal di `include/tb_classify.h`. Adapter yang memanggilnya; `tb_triage_selftest.c` yang memakunya — dengan `predict_triage()` **versi stub milik selftest sendiri**, sehingga pemetaannya teruji tanpa me-link model 72k baris. Itu sebabnya file itu dulu tidak pernah teruji.

`tb_triage_classify()` juga melaporkan **ESI mentah** karena warnanya tidak bisa dibalik: 3, 4, dan 5 sama-sama HIJAU, jadi log yang cuma membawa warna tidak bisa membedakan pasien yang bisa berjalan dari yang di ambang. Dipakai untuk diagnosa saja — tidak ada yang menampilkannya di layar.

`ui_priority_t` urutannya `RED=0, YELLOW=1, GREEN=2, BLACK=3` — **bukan** urutan severity untuk BLACK. Cast langsung dari ESI memetakan pasien paling kritis ke warna paling bisa ditunda. Selalu lewat pemetaan di atas.

### Tiga input manual, dan yang ketiga bisa memaksa MERAH

Alur registrasi: **Age → Gender → Airway → Mengukur**.

`airway_problem` adalah satu-satunya input yang bisa **memaksa MERAH sendirian** (`tb_classify.h`), dan tidak ada sensor di alat ini yang bisa melihat jalan napas tersumbat — jadi memang harus dijawab operator. Layarnya `ui/logic/ui_airway.c`.

Aturan yang dipatok selftest, semuanya soal "jangan sampai MERAH ter-set tanpa niat":

- Baris **"Tidak ada" yang pre-highlight**, bukan "Ada". Default yang aman untuk sebuah *highlight* adalah jawaban yang umum, bukan yang paling hati-hati: ia hanya di-commit oleh Select, dan pre-select "Ada" berarti dua tekanan tergesa menandai pasien yang bisa berjalan sebagai MERAH.
- **Menggeser highlight bukan menjawab.** Sentuhan hanya memindahkan fokus, sama seperti Age dan Gender. Back juga tidak commit — highlight yang tertinggal di "Ada" lalu Back tidak boleh mencatat pasien sebagai tersumbat.
- **`ui_session_has_airway()` terpisah dari nilainya.** "Belum ditanya" dan "sudah ditanya, jawabnya tidak" tidak boleh terlihat sama.
- **Tidak menyelamatkan penolakan jadi MERAH.** Tanpa vital sama sekali hasilnya tetap HITAM: alat tidak tahu sedang melihat apa, dan operator yang sudah melihat jalan napas tersumbat tidak butuh izin layar untuk bertindak.
- Sesi baru mengosongkannya lagi, highlight sekalian — jalan napas tersumbat pasien sebelumnya tidak boleh ikut ke pasien berikutnya.

Log `triage:` membawa `esi` **dan** `airway`, karena itu yang membedakan MERAH pilihan model (`esi=1`) dari MERAH paksaan operator (`airway=1 esi=5`).

#### Kenapa layarnya di-*hand-build*, bukan XML

Layar baru butuh `*_gen.c` **dan** entri di `ui_gen.c` yang di-generate, dua-duanya keluar dari Editor lewat manusia menekan Ctrl+B. Meng-edit file generated adalah satu aturan yang tidak ditawar di repo ini, jadi alternatifnya menunggu. `ui_airway.c` meng-instantiate **komponen generated yang sama** (`status_bar_create()`, `button_bar_create()`) dan menyalin style `gender.xml`, jadi visualnya identik dan **ButtonBar fisiknya bekerja**.

Yang terakhir itu bukan detail: alat ini dioperasikan dari 4 tombol fisik, dan dialog akan jadi touch-only. Kalau nanti layar Airway di-author di Editor: hapus `ui_airway.c`, buang case-nya dari `screen_root()`, sisa wiring-nya (ui_nav, ui_action, session) tidak berubah.

### Yang belum terhubung

- **Tekanan darah.** `bp_pipeline.c` + `lgbm_sbp.c` (estimator SBP dari PPG) ada di build tapi belum dipanggil dari mana pun. Sementara ini `systolic_bp` datang dari snapshot STM32, yang `TB_FLAG_BP_VALID`-nya masih selalu 0 — jadi praktisnya 0, dan gate "menolak menilai" yang menangkapnya.
- **Age band, bukan umur.** UI hanya mengumpulkan band, jadi `tb_triage_age_years()` memilih titik tengah (12/31/53/70). Error terikat setengah lebar band, bukan selebar band.
- `reasons` dikirim kosong. Backend memang men-default ke `[]`.

### `-mauto-litpools`: kenapa ada flag khusus di CMakeLists

`score_lgbm_multiclass()` adalah **satu fungsi ~67k baris**, dan itu memecahkan build Xtensa: `dangerous relocation: l32r: literal target out of range`. `l32r` hanya menjangkau mundur sejauh tertentu, sedangkan semua literal satu fungsi normalnya ditaruh di satu pool di awal section — ujung jauh kodenya terlalu jauh dari pool itu.

`-mtext-section-literals` **tidak cukup**: errornya hanya pindah dari linker ke assembler (`operand 2 of 'l32r' has out of range value`), karena masih satu pool per section. `-mauto-litpools` membiarkan assembler menyisipkan pool tambahan di tengah fungsi setiap kali jangkauannya habis. Flagnya di-scope ke satu file itu saja. Hilang sendiri kalau modelnya di-emit sebagai tabel data + interpreter kecil, yang sekaligus memotong 72k baris dan waktu compile-nya.

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

### Boot hang setelah soft reset: STM32 menahan SCL (terukur)

Gejala: log boot berhenti **tepat** di antara dua baris ini dan tidak ada apa pun setelahnya — tidak ada panic, tidak ada watchdog, tidak ada heartbeat (heartbeat tiap 10 s, jadi diamkan ≥25 s sebelum menyimpulkan):

```
I (1280) GT911: I2C address initialization procedure skipped - using default GT9xx setup
                        <-- berhenti di sini
I (1281) GT911: TouchPad_ID:0x39,0x31,0x31
```

**Penyebab, sudah diukur bukan dikira-kira.** Sebelum `i2c_new_master_bus()` mengambil pin, `bsp_i2c_bus_recover()` membaca level kedua jalur sebagai GPIO. Hasilnya konsisten **4/4 percobaan reset: SCL LOW, SDA high**.

- **SCL** ditahan low = ada slave yang **clock stretching**. Master tidak bisa mengangkat jalur yang ditahan device lain, jadi ini **tidak bisa dipulihkan dari sisi ESP32** — 9 pulsa recovery pun tidak bisa, karena yang harus dipulsakan itu justru jalur yang ditahan.
- Yang bisa stretch di bus ini hanya STM32 (`0x42`). GT911/TCA9554/SW6106/PCF85063A tidak. Konfirmasi satu langkah: **cabut STM32, reset lagi — kalau boot normal, terbukti.**
- Mekanismenya: ESP32 reset di tengah transfer (setiap `idf.py flash`, setiap pulsa RTS, setiap panic — dan ESP32 jauh lebih sering reset daripada STM32). Slave STM32 tertinggal di tengah transfer, menahan SCL menunggu clock dari master yang sudah tidak ada. Ditahan **selamanya**.

**Kenapa hang, bukan error:** `esp_lcd_panel_io_i2c` mengirim timeout `-1` ke `i2c_master_transmit_receive()` (`esp_lcd/i2c/esp_lcd_panel_io_i2c.c:145`, IDF v6.0.2), artinya tunggu tanpa batas. Jadi read config GT911 yang pertama memblokir permanen tanpa error, tanpa log, tanpa watchdog.

**Workaround sekarang:** power cycle (cabut-pasang daya / tombol power). Reflash **tidak** menolong — sudah diuji, langsung balik ke baris GT911 yang sama.

**Perbaikan sebenarnya ada di sisi STM32**, dan hanya ada di situ:

1. Jangan stretch tanpa batas — beri timeout pada transfer slave, dan reset blok I²C (`I2C_CR1_SWRST` di F4) kalau transfer mandek.
2. Handler slave jangan mengerjakan apa pun yang lama. Siapkan buffer snapshot di luar ISR supaya ISR hanya menyalin byte; stretching yang panjang itulah yang membesarkan jendela kena reset.
3. Paling bersih: DMA untuk transfer slave, sehingga tidak perlu stretch sama sekali.

Alternatif hardware kalau (1)–(3) tidak jalan: sambungkan `NRST` STM32 ke satu GPIO ESP32 yang bebas, supaya ESP32 bisa mereset STM32 saat mendeteksi SCL low. Belum dilakukan — butuh satu pin dan satu kabel.

Sisi ESP32 sekarang **hanya melaporkan**, tidak memperbaiki: `bsp_i2c_bus_recover()` mencetak `I2C SCL held LOW -- a slave is clock-stretching ...` lalu boot tetap hang di GT911. Itu perbaikan dari hang yang benar-benar bisu. Recovery 9-pulsa untuk kasus **SDA** nyangkut (AGENTS.md item 20) tetap ada di fungsi yang sama, cuma ternyata bukan itu bug-nya.

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

## Menu (tombol 3) dan mode demo

Tombol **Menu** (cell 3 di ButtonBar) — yang di flow Figma tidak punya arti — membuka **daftar setting**, satu baris per setting: judul + sublabel penjelas di kiri, switch di kanan. Isinya sekarang satu: mode demo. Berikutnya toggle tema terang/gelap.

Bentuknya daftar, bukan sepasang tombol, karena versi pertama (dua tombol: `Tutup` / `Demo: Aktifkan`) tidak bertahan begitu ada entri kedua — jadinya tiga tombol berjejer yang dua di antaranya setting dan satu navigasi, dan label tombol harus membawa state (`Demo: Matikan` berarti demo sedang **nyala**, terbalik dari cara label biasa dibaca). Switch menunjukkan state langsung. Menoggle **tidak** menutup dialog: dengan lebih dari satu setting, menutup tiap kali berarti membuka menu lagi untuk mengubah yang kedua. Kartunya `LV_SIZE_CONTENT`, jadi menambah baris tidak perlu hitung-hitungan ukuran.

Tombol Menu juga **menutup** dialognya sendiri kalau ditekan lagi. Ini bukan kemewahan: baris dan tombol `Tutup` adalah target sentuh, jadi tanpa ini orang yang mengoperasikan alat dari 4 tombol fisik bisa membuka menu dan terkurung di belakang scrim. Konfirmasi Power sengaja **tidak** begitu — itu harus dijawab, bukan dibubarkan tekanan nyasar.

Switch demo berwarna **kuning**, bukan hijau accent. Setiap switch lain di menu ini akan berupa preferensi biasa; yang ini membuat alat melaporkan triase yang tidak diukur siapa pun, jadi tidak boleh terlihat seperti preferensi. Hijau accent di palet ini juga **sama dengan triage GREEN** — warna terakhir yang boleh dipinjam kontrol bertuliskan "hasil palsu".

Baris `Tema` **belum dibuat**, sengaja: switch yang tidak melakukan apa-apa mengajari operator bahwa menunya rusak, dan desain mode terang memang dijadwalkan nanti.

### Mode demo (untuk video)

| | Demo ON | Demo OFF |
| --- | --- | --- |
| 4 tanda vital | **palsu** — HR ~128, SpO₂ ~88, RR ~32, TD ~86/54, bergoyang tiap ~0,8 s | dari sensor |
| Hasil triase | **MERAH**, confidence 0,93 — model **tidak** dijalankan | model GBM |
| Dot Sensor di Home | **hijau semua** | apa adanya |
| RFID, tombol fisik, baterai, dot Sistem/LoRa, **RSSI** | **asli** | asli |
| Frame `RESULT` ke STM32 | **dikirim** (station + dashboard ikut lihat pasien demo) | dikirim |

Angka palsunya sengaja dibuat **konsisten dengan MERAH** — HR 72 / SpO₂ 99 di sebelah label MERAH justru terlihat seperti alat rusak. Digoyang sedikit supaya tidak seperti screenshot, tapi deterministik (tabel, bukan random) sehingga bisa dipatok selftest.

Yang **tidak** dipalsukan: RFID. Jadi STM32 harus tetap tersambung untuk merekam — layar Scanning menunggu tag betulan. RSSI juga tidak dipalsukan: gunanya justru mengukur jarak nyata, jadi angka palsu di situ mematikan fiturnya.

**Mati sendiri setiap boot**, tidak disimpan ke NVS. Ini disengaja: kalau lupa dimatikan sesudah rekaman, satu power cycle sudah cukup untuk mengembalikan alat ke normal, bukan alat yang menyebut semua pasien MERAH. Tidak ada badge "DEMO" di layar karena itu justru merusak videonya — gantinya reset-saat-boot di atas plus `ESP_LOGW DEMO MODE: reporting priority=0, model not run` di setiap hasil.

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
| `demo [0\|1]` | Mode demo on/off (toggle kalau argumen dikosongkan) — sama dengan dialog Menu, tapi tanpa tangan masuk frame |
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
