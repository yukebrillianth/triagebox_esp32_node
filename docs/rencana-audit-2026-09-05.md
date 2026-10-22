# Rencana eksekusi — hasil audit 5-seam, 2026-09-05

Dokumen ini adalah rencana kerja yang bisa dieksekusi dari konteks kosong. Semua
temuan sudah diverifikasi terhadap kode di disk; setiap item menyebut file dan
alasannya, jadi pelaksana tidak perlu mengulang audit.

**Status saat rencana ini ditulis:** belum ada satu file pun yang diubah sejak
audit dimulai. `bash tools/run_selftests.sh` hijau (12 suite), ESP-IDF build
hijau, STM32 build hijau. Working tree kedua repo firmware kotor dari sesi-sesi
sebelumnya — itu normal untuk proyek ini dan bukan bagian dari rencana ini.

## Aturan yang tidak boleh dilanggar

1. **Satu file, satu pemilik.** Tabel kepemilikan di §Fase adalah kontrak. Dua
   agent yang menulis file sama pernah menabrakkan definisi enum di proyek ini
   dan baru terlihat saat link. Agent yang butuh perubahan di file milik orang
   lain **melaporkannya**, tidak mengeditnya.
2. **`tb_regs.h` ada dua salinan** (`components/triagebox_link/include/` dan
   `triagebox-stm32-node/Core/Inc/`) yang wajib byte-identical. Rencana ini
   hanya mengizinkan perubahan KOMENTAR di sana, disalin ke keduanya, tanpa
   menaikkan `TB_PROTO_VER`. Perubahan layout apa pun di luar lingkup.
3. **`triagebox-station/` read-only.** Milik penulis lain. Temuan yang
   menyentuhnya jadi draft change request, bukan edit.
4. **Verifikasi per fase**, bukan di akhir: `bash tools/run_selftests.sh` lalu
   `idf.py build`. STM32: `sh tools/run_selftests.sh` lalu `make` dengan
   `PATH=$HOME/.local/toolchains/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi/bin:$PATH`.
5. **Jangan commit** tanpa diminta. Laporkan diff, biarkan user memutuskan.

## Konteks yang menentukan prioritas

- Alat ini dipakai demo di depan penguji. Temuan yang terlihat dalam 30 detik
  didahulukan atas temuan yang benar tapi tak terlihat.
- BP dari model ML, bukan cuff; satu perbandingan cuff menunjukkan ~27 mmHg
  lebih rendah. SpO2 kurva tak terkalibrasi. RR diketik operator. Ketiganya
  bukan angka klinis, dan UI tidak boleh mengklaim sebaliknya.
- **Penolakan model saat ini terbit sebagai HITAM/EXPECTANT** dengan alarm, ke
  layar dan ke station. Itu temuan nomor satu dari lima audit; diperbaiki oleh
  agent A2 (§Fase 1).
- **Jam status bar tidak pernah tampil** karena tidak ada driver RTC dan tidak
  ada yang memanggil `settimeofday()` — bukan karena baterai RTC. Baterai
  sekarang sudah dipasang, jadi kerjanya murni firmware; agent A4.

## Fase 1 — 4 agent paralel, file sepenuhnya disjoint

### A1 — Window BP & capture
**Milik:** `components/triagebox_link/bp_capture.c`, `include/bp_capture.h`,
`bp_capture_selftest.c`, `ui/logic/ui_mock.h`

1. `BP_MIN_SAMPLES` 4000 → **3000**. Alasan: window 45 s × 98,97 Hz = ~4453
   sampel, floor 4000 menyisakan 453 sampel = **4,6 detik** toleransi gap, dan
   `tb_link_i2c.c:423-431` me-restart **seluruh** akumulator pada satu sampel
   yang hilang. Sebelum window dipendekkan: 5938 vs 4800 = 11,5 detik. 3000
   memulihkannya ke 14,7 detik. Bukti 30 s masih layak: output
   `tools/bp_window_sweep.c` (30 s → +3,4/−4,7 mmHg vs referensi 60 s).
2. `_Static_assert` yang mengikat floor ↔ window ↔ 98,97 Hz. **Harus** membaca
   konstanta BARU `UI_MEASURE_MS_DEFAULT` di `ui_mock.h` yang `-D` tidak bisa
   menggeser — `run_selftests.sh:22` dan `sim/CMakeLists.txt:162` dua-duanya
   meng-override `UI_MEASURE_MS` ke 2000. Assert kedua: `BP_WINDOW_SAMPLES`
   cukup untuk apa pun yang `bp_capture_record()` boleh minta. Ini test dengan
   nilai tertinggi dari seluruh audit: kelas bug yang ditangkapnya adalah gate
   4800-of-5937, di mana BP diam-diam tidak pernah terbit, triase mengimputasi
   129,7 untuk semua pasien, dan alatnya tetap kelihatan bekerja.
3. Komentar salah: `bp_capture.c:113` menulis filter ECG "Q 0.75", koefisien
   yang dikirim Q = **0,866** (`bp_capture.h:83` sudah benar). Terbukti
   aritmetis: w0 = 2π·8,6603/100, α = sin(w0)/1,732 → kelima nilai persis.
4. `BP_FS_HZ 100.0f` (`:27`) bertabrakan dengan 98,97 Hz sebelas baris di
   bawahnya. Jelaskan mana yang dipakai di mana (`bp_hr_from_ecg`,
   `bp_capture_record`), ~1% error.
5. `include/bp_capture.h:50` masih bilang "48 s floor" → 30 s.
6. `bp_hr_from_ecg()` tanpa guard `n == 0` — `:203` membaca `ecg[0]`.
   `bplog <n>` tanpa data wave + pengukuran berjalan menjangkaunya di atas
   `malloc(0)`.
7. `bplog` **tolak arm saat pengukuran aktif**. `bp_capture_record()` memanggil
   `bp_capture_start()` dari task console sementara `s_count`/`s_capturing`/
   `s_acc` adalah static biasa non-volatile yang dipakai tiga task tanpa
   primitif apa pun; hari ini `bplog <detik>` di tengah pengukuran bisa membuang
   BP pasien tanpa pesan, atau membersihkan `s_capturing` di tengah window.
8. Ownership map di `bp_capture.h:14-18` **salah** — perbaiki: `record()` dan
   `wave_push()` dua-duanya melanggarnya.

**Verifikasi:** `bash tools/run_selftests.sh` (assert baru harus lulus pada
default 45000 **dan** pada override 2000).

### A2 — Verdict & identitas (triase, nav, runtime)
**Milik:** `components/triagebox_ml/tb_triage_model.c`, `ui/logic/ui_nav.c`,
`ui_nav_selftest.c`, `ui_runtime.c`, `ui_runtime_selftest.c`, `ui_types.h`,
`ui/logic/ui_status.c`, `ui_status.h`, `ui_status_selftest.c`

1. **Penolakan tidak boleh jadi HITAM — perbaikan tunggal tertinggi.**
   Ketika `tb_triage_classify()` menolak (`!v->valid` atau gate `<= 0`):
   kirim `LORA_VITAL_PRIORITY_NONE` (0xFF) ke `TB_REG_PRIORITY` alih-alih wire
   BLACK (0). **Jalur sudah diverifikasi bersih**: `tb_slave.c:607-609` menerima
   byte apa pun tanpa validasi, `main.c:825` menyalunya mentah,
   `lora_vital_priority_name(0xFF)` → NULL → station menahan seluruh vital,
   status node tetap ONLINE. Nol perubahan wire, nol perubahan STM32/station.
   Konsekuensi yang harus ditangani di sisi layar: Result harus menampilkan
   keadaan "TIDAK TERUKUR" yang jujur (label + banner netral, **bukan**
   "HITAM - EXPECTANT", tanpa bunyi alarm) alih-alih membiarkannya jadi hitam
   dengan ESI 0. `ui_runtime.c:45-48` sudah punya logika kebenaran ini untuk
   re-triage — pakai sebagai rujukan.
2. **Tag verdict = tag sesi, bukan tag hidup.** `tb_ui_source.c:681` membaca
   `s_rfid` yang ditimpa setiap poll begitu gate terbuka, sementara layar
   menampilkan `ui_session_get_rfid()`. Kartu kedua yang tersenggol reader di
   tengah pengukuran → hasil terkirim di bawah ID salah. Ini kelas bug yang
   gate RFID ada untuk mencegahnya. Perbaikan: `infer_once()` mengirim tag dari
   salinan sesi yang di-latch saat scan (koordinasi ringan dengan A3 yang
   memiliki tb_ui_source.c — lihat §Jembatan di bawah).
3. **Blok end-session di `ui_nav.c:93-97` adalah kode mati** — dari Result target
   nav hanya MONITOR/HOME/TEST, dari Monitor hanya RESULT/TEST, semua dikecualikan
   kondisinya sendiri. Hapus bloknya dan selftest yang memin jalur mati
   (`ui_nav_selftest.c` test_leaving_result_or_monitor_ends_session), ATAU
   pertahankan sebagai defense-in-depth dengan komentar jujur. Putuskan, jangan
   biarkan komentar overstate.
4. **Dot Sensor membawa nol informasi.** Dua sisi: (a) `UI_SENSOR_ALL`
   (`ui_status.h:38-39`) menyertakan `UI_SENSOR_MIC` yang `MON_RESP_MIC_FITTED 0`
   tidak pernah set → dot tak pernah hijau; (b) `ui_status_sensors()` tidak
   menerima umur, dan `tb_ui_source.c` menyerahkan mask terakhir tanpa batas →
   dot tak pernah memerah setelah board mati. Perbaikan: keluarkan MIC dari mask
   sampai mic ada (satu baris + perbaiki selftest), dan buat mask menua bersama
   link (pola `LINK_STALE_MS` 45 s yang sudah ada untuk dot Sistem).
5. **Label `ui_status.h:56` menjanjikan "Sensor 3/5"** tapi `ui_status.c:134-139`
   cuma emit `OK`/`!`/`--`. Samakan janji dan kenyataan (dot + tooltip).
6. **Hapus alarm palsu saat boot**: `ui_runtime_init()` berjalan sebelum
   `tb_link_start()` → `ui_mock_end_session()` mencetak
   `"ABORT not sent -- STM32 may keep reporting this patient"` setiap boot.
   Cukup: `ui_mock_init()` meng-clear status sebelum end_session, atau urutan
   init dibalik di `app_main.c` (lihat §Jembatan dengan A3).
7. `ui_types.h:14` "BP has no source at all yet, so its bit is always clear" —
   basi. Perbaiki.
8. Test `ui_rr_band_value()`: tiga nilai band lolos dari mutasi di SEMUA suite
   yang menautkan `ui_types.c`, padahal itu `TriageInput.respiratory_rate`,
   input yang penolakannya terjadi di `tb_classify`. Tambahkan assert di
   `ui_runtime_selftest.c` (atau selftest ui_types bila ada) yang memastikan
   keempat band memetakan ke nilai mid-point klinisnya (12, 16, 25.5, 35 —
   konfirmasi angka aktual dari `ui_rr.c`/`ui_types.c` sebelum menulis).
9. Test `measured_vitals` yang memang menangkap mutasi: `ui_runtime_selftest.c:192-197`
   hanya meng-assert dua mask bit. Tambahkan: tangkap vitals pada measure-done,
   jalankan round-trip Monitor, assert `m->hr`/`m->spo2` tidak berubah.
   Membunuh 6 mutasi hidup dan membuat komentar di `ui_runtime.c:165-187` benar.

**Verifikasi:** `bash tools/run_selftests.sh` + `idf.py build`.

### A3 — Dashboard: buang yang tidak relevan
**Milik:** seluruh `triagebox-dashboard/`

**HAPUS — node id dan sekitarnya (keluhan utama user).** Node id dibuat backend
saat adopsi (`stations.service.ts:173-177`, `node-${nodeIdBase+i+1}`) dan
firmware station memetakan `node_id` biner ke string itu persis. Id yang diketik
manual **dijamin mati**: bukan format `node-NN` → tidak pernah menerima paket;
`node-01..10` → 409 karena sudah dipakai seed.
- `app/(app)/perangkat/container.tsx:465-696` `NodeFormDialog` seluruhnya;
  `:1079-1084` tombol `+ Node`; `:610-625` field **ID**; `:670-679` field
  **Firmware** node (station cuma publish `status/rssi/snr/packet_count/battery`);
  `:627-652` select **Station** untuk re-parent; `:1198-1221` Edit/Hapus node —
  Hapus itu bohong, `ingest.service.ts:67-69` mengaktifkan ulang node yang
  di-soft-delete pada vital berikutnya.
- `schemas/device.ts:60-71`, `lib/api.ts:202-220`, `types/index.ts:233-244`.
- Field station yang tidak dibaca siapa pun: `:270-306` MQTT Host+Port (broker
  dari `process.env.MQTT_URL`), `:76-81` `mqttLabel`, `:1006-1008` kolomnya;
  `:254-268` IP dan `:308-317` Firmware sebagai **input** (kolom read-only
  dipertahankan — station mengumumkannya, tapi `handleAnnounce` return awal
  untuk station terdaftar sehingga hanya bisa diketik ulang manual);
  `:221-236` field **ID** station (adopsi menyediakannya).
- **Kolom RR** yang kosong secara struktural: `pasien/container.tsx:119-127`,
  `:423-425`; `command/container.tsx:154-160`; `pasien/[id]/container.tsx:314-321`,
  `:398-400`, `:440-446`. Verifikasi keras: **tidak ada `TB_REG_HOST_RR`** —
  register host cuma `HOST_BATTERY`/`HOST_BP_SYS`/`HOST_BP_DIA`, jadi RR yang
  diketik operator tidak punya jalan ke wire dan `TB_FLAG_RR_VALID` hanya diset
  dari mikrofon yang tidak ada.
- Mati tanpa importer: `constants/index.ts`, `services/index.ts`, `hooks/index.ts`,
  `providers/index.ts`, `components/triage/ConnectionBadge.tsx` (diduplikat
  verbatim di `AppTopbar.tsx:12-32,136-155`), plus export tak terpakai di
  `hooks/use-api.ts:97,117`, `lib/api.ts:198,235`, `lib/config.ts:10`,
  `lib/theme.ts:33`, `components/triage/formatters.ts:22`, `schemas/device.ts:46,73`,
  `types/index.ts:6,115,120,137`. Perbarui `e2e/misc.spec.ts:65,70` yang
  meng-assert `btn-create-node`/`btn-create-station`.

**NULL-HANDLING (crash nyata, sudah diverifikasi).** `Victim.currentPriority`
adalah `Priority?` di `schema.prisma:72` tapi ditype non-nullable di
`types/index.ts:22`:
- `pasien/container.tsx:205` `counts[v.currentPriority] += 1` → **NaN** di kartu
  ringkasan.
- `PRIORITY_RANK[null]` → `undefined` → comparator rusak, di `pasien:218-219`
  **dan** `command:262` (`PRIORITY_RANK` diduplikat di kedua file, `pasien:38`
  dan `command:25`).
- `report:24-27` dan `pasien/[id]:48-51` sudah menjaganya — salin polanya.
- Hapus `Victim.hr/spo2/rr/bpSys/bpDia/battery/lastSeen` dari `types/index.ts:31-38`:
  **kolomnya tidak ada** di `schema.prisma` (model Victim hanya punya
  `currentPriority`, `confidence`, `reasons`, `currentNodeId`, `firstSeen`,
  `lastUpdate`, plus `name/age/gender/notes`). Angka vital hanya hidup dari patch
  `vital.updated` dan tersapu setiap `runBootstrap`/`invalidateQueries` — jadi
  refresh halaman saat demo mengosongkan semua vital sampai poll berikutnya.
  Dokumentasikan perilaku itu di komentar, jangan pura-pura ada.
- Rename yang tidak pernah cocok: `lastSeen` → `lastUpdate`,
  `VitalReading.createdAt` → `receivedAt`, `activityTimeline` → `recentActivity`.

**RELABEL — kejujuran angka.** `formatters.ts:100` menerapkan ambang klinis
`<90` merah / `<95` kuning pada SpO2, dan `VitalCell.tsx:10` menyebutnya
"clinical thresholds". Kurvanya `110−25R` tak terkalibrasi dengan R menempel di
clamp dan nol tes pembanding → label `SpO2 (tren)` dan buang eskalasi warnanya.
BP dirender `120/80 mmHg` seperti hasil ukur (`pasien/[id]:323-341`, `pasien:129`)
padahal prediksi ML → `BP (prediksi ML)`. Tirulah nada `pasien/[id]:487`
"Prioritas ML (readonly)" yang sudah benar.

**REALTIME:** `RealtimeProvider.tsx:356-359` `onAlertCreated` menyisipkan ke
**setiap** cache `['alerts','list',*]` termasuk filter yang tidak cocok;
`:382-390` memfabrikasi `onlineNodes/totalNodes: 0` saat cache KPI dingin,
padahal `kpi.updated` tidak pernah menyala pada transisi node/station.

**JANGAN SENTUH:** blok adopsi pending-station + `Offset ID node` (`nodeIdBase`
nyata), kolom RSSI/SNR/battery/packet-count, Analytics, Alert Center + ack,
Settings, jalur cetak Report, konvensi staleness per-cell, `formatBp`, dan
ketiadaan GPS/peta/entri vital manual.

### A4 — Power/boot/console + jam RTC
**Milik:** `main/app_main.c`, `components/triagebox_board/ui_board.c`,
`ui_board_power_selftest.c`, `components/triagebox_debug/tb_debug.c`,
`components/triagebox_board/` (driver RTC baru bila dibuat)

1. **Dua jalur boot-loop dengan panel gelap.** `app_main.c:154`
   `ESP_ERROR_CHECK(mount_assets())` dengan `format_if_mount_failed=false`, dan
   tiga `ESP_ERROR_CHECK` pada write I2C di `ui_board.c:281-284`. Dengan
   `PANIC_PRINT_REBOOT` + `REBOOT_DELAY_SECONDS=0`, keduanya abort-reboot
   instan selamanya, backlight TCA9554 tertinggal menyala — persis gejala
   "blackscreen, backlight nyala" yang tercatat di memori proyek. Kondisi yang
   menjangkau jalur kedua: STM32 menahan SCL setelah GT911 (bug terukur).
   Perbaikan: degradasi anggun — log keras, lanjutkan tanpa fitur yang gagal,
   sesuai pola jalur error lain di `ui_board.c` yang sudah return dengan sopan.
   `app_main.c:165-169` (cabang `disp == NULL`) adalah dead code karena
   `CONFIG_BSP_ERROR_CHECK=y` membuat BSP abort internal — jangan diandalkan.
2. **Baterai berbohong ke arah salah.** `ui_board.c:169-171`: setelah
   `soc &= 0x7F`, nilai 101-127 di-clamp ke 100 dan return sukses. Baca 0xFF
   (bus melayang/NAK) → tampil 100% + ikon penuh, dan `UI_BATTERY_UNKNOWN`
   tidak bisa pernah sampai karena mask membuang bit 7. Out-of-range →
   `return false`. Ada selftest `ui_board_power_selftest.c` untuk pola ini.
3. **Demo mode harus jujur.** `s_enabled` static di luar struct session →
   bertahan melewati reset sesi, tidak ada indikator di layar setelah Menu
   ditutup, dan `tb_ui_source.c:685-689` tetap mengirim RESULT MERAH/0,93/ESI 1
   ke station sebagai catatan asli. Perbaikan: (a) reset `s_enabled` saat sesi
   berakhir (panggilan sudah ada di `ui_mock_end_session`), (b) penanda
   permanen di status bar selama aktif, (c) jangan kirim RESULT dalam demo
   ATAU tandai payload-nya. **A3 milik A4** karena menyentuh `tb_ui_source.c`
   — koordinasi via §Jembatan.
4. **Jam tidak tampil karena jam tidak pernah diset — RTC-nya belum ada.**
   Verifikasi keras: tidak ada driver RTC sama sekali (0 hit `settimeofday` di
   seluruh tree), padahal PCF85063A di 0x51 tercatat di bus
   (`tb_regs.h:42`), `EXIO7=RTC_INT` tercatat di `ui_board.c:19`, dan user
   baru saja memasang baterai RTC. Formatter UI sudah SIAP:
   `ui_status_format_clock_at()` (`ui_status.c:67-86`) menampilkan `--:--`
   sampai jam melewati `UI_CLOCK_VALID_EPOCH` (2025-01-01) — komentarnya bahkan
   bilang "fitting the RTC battery and setting the time is all it takes, no
   code change". **Tapi itu hanya benar kalau ada yang menulis jam.** Kerjakan:
   - Driver kecil PCF85063A di `components/triagebox_board/` (baca/tulis
     BCD time registers, pola `sw6106_read` yang benar: `bsp_i2c_lock()`
     diambil sendiri — JANGAN pola `ui_board_battery_mv` yang membaca dua
     register tanpa lock; itu bug terpisah yang diketahui dan TIDAK dikerjakan
     di rencana ini demi risiko demo).
   - Saat boot: baca RTC → `settimeofday()`. Tanpa TZ: `setenv("TZ","WIB-7",1);
     tzset()` sekali di init (WIB = UTC+7; format `WIB-7` adalah POSIX, tanda
     terbalik itu benar).
   - Tidak ada SNTP/WiFi di firmware ini — RTC adalah satu-satunya sumber jam.
   - Lokasi panggilan: `app_main.c` setelah bus siap, sebelum UI start.
   - Tombol pengeset waktu TIDAK dikerjakan (bisa lewat console debug
     `time set HH:MM` di `tb_debug.c` sebagai utilitas, opsi).
5. **Console yang berbohong atau menggantung** (`tb_debug.c`): `i2c -1` →
   `atoi` tanpa validasi → `portMAX_DELAY` + WDT mati = hang permanen (`:152`);
   `btn 256` cast sebelum validasi → tombol 0 → Scan (`:108`); `status` tanpa
   argumen menghijaukan dot Sensor tanpa kedaluwarsa (`:76-80`); `rfid`/`vital`/
   `status` memanggil `mark_frame()` menghijaukan Sistem 45 s tanpa STM32;
   empat perintah I2C melewati `bsp_i2c_lock()` (`:176,336,402,481`) —
   `i2cdump 0x3c` = 256 transaksi tanpa lock; `stats` mencari task `"tb_rx"`
   padahal namanya `"tb_i2c"` sehingga angka overflow stack tak pernah dicetak.
6. **`bp_capture_init()` return void** (`bp_capture.h:22`) → `app_main.c:196`
   tak bisa memeriksa; gagalnya berarti `infer_once()` membakar 2000 ms penuh
   per pengukuran dengan LVGL beku. Ubah ke `esp_err_t`, tangani di `app_main.c`.
   **A4 milik A4** (menyentuh `bp_capture.h` yang milik A1 — §Jembatan).
7. **Abort meninggalkan wave capture jalan**: `ui_mock_end_session()`
   (`tb_ui_source.c:490-492`) membersihkan flag ukur tanpa
   `bp_capture_measure_done()` → `s_capturing` tetap true → transaksi ~25 ms
   tiap 50 ms di bus GT911, selamanya. **A4 milik A4** (tb_ui_source.c, §Jembatan).
8. `ui_board_power_off()` return dari tujuh jalur tanpa pemeriksa (`ui_board.h:86`
   mengklaim satu), dan `tb_ui_source.c:809` sudah menyuruh STM32 park sensor +
   diam SEBELUM memanggilnya. Balik urutan: panggil `power_off` dulu, baru
   kirim TB_CMD_POWER_OFF — atau tampilkan kegagalannya di layar.

**Verifikasi:** `bash tools/run_selftests.sh`, `idf.py build`. Jam RTC: butuh
board — tulis `console` smoke command (`rtc read`) yang bisa dijalankan user
saat board terpasang; jangan buat `idf.py monitor` bagian dari verifikasi
otomatis.

### §Jembatan — file yang disentuh lebih dari satu agent

Tiga file di garis batas. Kepemilikan **final**:

| File | Pemilik | Yang boleh dilakukan orang lain |
| --- | --- | --- |
| `components/triagebox_link/tb_ui_source.c` | **A3 tidak. A4 YA.** | A4 menangani: demo-result gating (3), abort→measure_done (7). A2 menangani tag-verdict (A2 §2) — kirim ke A4 sebagai PATCH TEKS, A4 yang mengedit |
| `components/triagebox_link/bp_capture.{c,h}` | **A1** | A4 §6 hanya mengubah signature `bp_capture_init()` + call-site `app_main.c`; koordinasikan dengan A1 agar tidak menulis bersamaan |
| `components/triagebox_link/include/tb_regs.h` | **Tidak ada yang mengubahnya.** | Hanya komentar, di KEDUA salinan, tanpa bump `TB_PROTO_VER` |
| `main/app_main.c` | **A4** | A2 boleh menyebutnya tapi urutan init (A2 §6) diserahkan A4 |

Sequencing: A1 dan A4 berjalan dulu untuk `bp_capture.h` (signature change),
A1 lanjut sendiri, lalu A2. Dalam praktik: mulai A1+A4 paralel, A2 setelah A1
selesai dengan bp_capture.h, A3 bebas sepanjang waktu karena repo terpisah.

### Fase 2 — Verifikasi terintegrasi

Dijalankan setelah keempat agent Fase 1 selesai dan §Jembatan diselesaikan.

1. `bash tools/run_selftests.sh` — semua hijau termasuk assert baru A1 dan test
   baru A2.
2. `idf.py build` — hijau, tidak ada warning baru.
3. `sh tools/run_selftests.sh` di `triagebox-stm32-node` + `make` — tetap hijau
   (rencana ini tidak menyentuh STM32, tapi jalurnya harus tetap benar karena
   working tree berisi perubahan ABORT→ForgetPatient yang belum di-commit dan
   **wajib di-flash** agar task #29 benar-benar bekerja).
4. `pnpm run lint && pnpm exec tsc --noEmit && pnpm run build` di dashboard.
5. Jam RTC: `idf.py flash monitor` dengan board terpasang — user verifikasi
   `rtc read` menampilkan waktu, lalu jam di status bar. Bukan bagian verifikasi
   otomatis.

### Yang sengaja TIDAK dikerjakan (dengan alasan)

- **NODE_ID dari tabel UID (STM32)** — sudah didiskusikan dengan user dan
  disetujui arahnya, tapi menyentuh firmware STM32 yang sedang membawa
  perubahan ABORT uncommitted; risiko demo menumpuk. Kerjakan SETELAH demo atau
  sebagai commit STM32 terpisah yang jelas.
- **TB_REG_HOST_RR / umur-gender-ESI di wire** — perubahan register map +
  `lora_vital.h` yang tersalin di repo read-only station. Change request.
- **Front-end filter BP ke spesifikasi training** (zero-phase Cheby2/Butter
  dari PKM_BP/config) — membatalkan gate flat-ECG + pin koefisien, butuh
  validasi on-device. Jangan sebelum demo.
- **`ui_board_battery_mv()` tanpa bus lock** — bug nyata, tapi di jalur demo
  hari ini; perbaikannya menambah risiko. Dicatat, bukan dikerjakan.
- **`CONFIG_TB_NODE_COUNT>1`, filter `station_id`, provenance HR di station,
  salinan `tb_regs.h` station** — repo read-only, change request.

### Flash & deliverable

- ESP32: `idf.py -p /dev/cu.usbmodem* flash monitor` (board belum terpasang
  saat rencana ini ditulis).
- **STM32 WAJIB di-flash** — tanpa itu ABORT tidak bekerja dan pasien terus
  dilaporkan ke station setelah keluar dari Result/Monitor.
- Dashboard: jalankan `pnpm run dev` dari `triagebox-dashboard/`, backend dari
  `triagebox-backend/` sesuai CLAUDE.md.




