# Integrasi UI TriageBox → ESP32 & STM32

> **Kalau kamu kerja langsung di repo `triagebox-lvgl`, lewati paket ini** — repo itu sekarang sudah berisi firmware ESP32 penuh (RS485 + SVM sudah ter-wire). Baca `docs/firmware-architecture.md`. Paket handoff ini untuk developer yang mengintegrasikan UI ke tree firmware lain.

Paket siap-pakai ada di **`handoff/triagebox-ui/`** (di-generate ulang dengan `tools/make_handoff.sh`). Isinya hanya C + aset; XML LVGL Pro dan TTF sumber tetap di repo UI.

Target: **Waveshare ESP32-S3-Touch-LCD-4 rev 3.0** (silkscreen!), 480×480, ESP-IDF v6.0.2, LVGL 9.5, `esp_lvgl_port` 2.9.

```
handoff/triagebox-ui/
  ui/                       277 file C hasil export LVGL Pro + logic layer (ui/logic/)
  ui/file_list_gen.cmake    daftar semua .c generated — include ini, jangan glob
  glue/asset_fs.[ch]        driver LVGL FS 'A:' (wajib, lihat §5)
  glue/app_main.example.c   contoh bring-up lengkap
  glue/CMakeLists.example.txt
  spiffs_assets/fonts/      7 TTF, harus di partisi `storage`
  partitions.csv
  sdkconfig.reference.defaults
  board/esp32_s3_touch_lcd_4/  BSP yang sudah dipatch untuk IDF v6 (lihat §7)
```

---

## 1. Pembagian kerja

| Milik | Isi |
| --- | --- |
| **UI (paket ini)** | 8 layar, ButtonBar, status bar, state machine flow triage |
| **ESP32 dev** | UART/RS485 RX dari STM32, inference SVM |
| **STM32 dev** | sensor (MAX30102 / AD8232 / MPX5010DP / RFID), 4 tombol fisik, kirim frame |

UI **tidak** punya driver sensor dan **tidak** baca GPIO tombol. Semua data masuk lewat satu seam: `ui_mock.h` (§3).

## 2. Kontrak data — `ui/logic/ui_types.h`

Ini satu-satunya sumber nama field. Field `vitals_t` sudah sama dengan key MQTT backend, jadi jangan di-rename di jalan.

```c
typedef struct {
    uint16_t hr, spo2, rr, bp_sys, bp_dia;
    uint8_t  battery;
    bool     valid;      /* false = data stale/invalid, UI tampilkan "--" */
} vitals_t;

typedef struct { uint8_t index; bool pressed; uint32_t timestamp_ms; } btn_event_t; /* index 0..3 */
typedef struct { char tag[RFID_TAG_CAPACITY /*32*/]; bool present; } rfid_t;

typedef enum { UI_PRIORITY_RED, UI_PRIORITY_YELLOW, UI_PRIORITY_GREEN, UI_PRIORITY_BLACK } ui_priority_t;
typedef enum { UI_AGE_BAND_6_17, UI_AGE_BAND_18_45, UI_AGE_BAND_46_60, UI_AGE_BAND_OVER_60 } ui_age_band_t;
typedef enum { UI_GENDER_M='M', UI_GENDER_F='F', UI_GENDER_U='U' } ui_gender_t;
```

`priority` / `confidence` / `reasons` **milik ESP32** (hasil SVM di ESP32). STM32 tidak pernah mengirimnya.

## 3. Titik integrasi: ganti `ui_mock.c`

Sekarang UI dijalankan oleh mock deterministik supaya bisa di-demo tanpa STM32. Integrasi = **implementasi ulang isi `ui_mock.c`** dengan data serial asli, header `ui_mock.h` **tidak diubah**. Semua screen dan navigasi tetap jalan tanpa disentuh.

| Fungsi di `ui_mock.h` | Yang harus dilakukan versi asli |
| --- | --- |
| `ui_mock_init()` | init UART + ring buffer |
| `ui_mock_tick(now_ms)` | drain frame yang sudah diparse RX task ke state internal |
| `ui_mock_start_scan()` | kirim perintah START_SCAN ke STM32 |
| `ui_mock_rfid_ready(rfid_t*)` | true **sekali** saat frame `RFID` masuk |
| `ui_mock_start_measure()` | kirim START_MEASURE |
| `ui_mock_measure_progress()` | 0..100 dari elapsed / 60000 ms |
| `ui_mock_measure_done()` | true saat window 60 s habis |
| `ui_mock_get_vitals(vitals_t*)` | isi dari frame `VITAL` terakhir; `valid=false` bila stale |
| `ui_mock_get_priority/confidence/reasons()` | hasil SVM, **bukan** hardcode |
| `ui_mock_push_button(index, pressed)` | dipanggil **RX task** saat state tombol berubah |
| `ui_mock_pop_button(btn_event_t*)` | dipanggil LVGL keypad indev — jangan dipanggil dari tempat lain |
| `ui_mock_get_link_status(link_status_t*)` | `sensor_mask` + `lora_ok` + umur snapshot terakhir |
| `ui_mock_power_off()` | kirim POWER_OFF ke STM32, tunggu, lalu cut rail via PMIC |

Transport-nya sekarang **I²C, bukan RS485**: ESP32 master, STM32 slave `0x42` di bus display, dan `components/triagebox_link/include/tb_regs.h` adalah register map yang berlaku (salinan verbatim di kedua repo, `TB_PROTO_VER` menjaga dari salinan basi). Ringkasannya di `AGENTS.md` §"Dual-MCU I²C contract", detail di `docs/firmware-architecture.md`. `tb_frame.h` masih ada tapi hanya untuk payload LoRa dan konversi prioritasnya.

Dua hal penting:

- **`UI_MEASURE_MS` default 2000 ms** (untuk QA). Hardware asli 60 s → set `-DUI_MEASURE_MS=60000` di build, atau `#define` sebelum include. Sama untuk `UI_MOCK_SCAN_MS`.
- **`ui_mock_push_button()` dipanggil dari task RX, bukan dari task LVGL.** Di `ui_mock.c` (sim) slotnya masih single-slot; di device (`tb_ui_source.c`) sudah ring buffer 8 event di dalam critical section, karena satu poll I²C bisa sah menghasilkan 4 edge sekaligus dan single-slot membuang semuanya kecuali yang terakhir — gejalanya "tombol yang kadang tidak ngapa-ngapain".

## 4. Aturan yang tidak boleh dilanggar

1. **`ui_action(screen, btn_id)` adalah satu-satunya dispatcher.** Touch dan tombol fisik keduanya lewat sini. Jangan buat jalur kedua — kalau ada, ButtonBar dan tombol fisik akan divergen.
2. **Jangan pernah edit `*_gen.c` / `*_gen.h`.** Itu hasil export LVGL Pro dan akan tertimpa. Perubahan tampilan → minta ke UI dev, bukan patch lokal.
3. **Screen tidak boleh dipanggil langsung.** Selalu `ui_nav_go(UI_SCREEN_*)`; `ui_nav.c` yang memanggil callback `lv_screen_load()` yang sudah diregister.
4. Semua mutasi objek LVGL dari task non-LVGL wajib di dalam `bsp_display_lock()` / `bsp_display_unlock()`.
5. Screen **membaca** `ui_session_*` — jangan simpan state layar sendiri.

## 5. Urutan bring-up (lihat `glue/app_main.example.c`)

```c
mount_assets();                       /* SPIFFS "storage" -> /assets */
ui_runtime_init();

bsp_display_cfg_t cfg = { .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG() };
cfg.lvgl_port_cfg.task_stack = 32768; /* WAJIB, lihat §6 */
disp = bsp_display_start_with_config(&cfg);

bsp_display_lock(2000);
asset_fs_init();                      /* WAJIB sebelum ui_init */
ui_init("A:/assets/");                /* trailing slash WAJIB */
ui_input_keypad_init(disp);
register_triage_screens();            /* 8x ui_nav_register */
lv_screen_load(home);
lv_timer_create(runtime_timer_cb, 50, NULL);  /* -> ui_runtime_tick(lv_tick_get()) */
bsp_display_unlock();
```

Build wiring: include `ui/file_list_gen.cmake` (jangan glob — file generated berubah tiap export), tambahkan 9 file `ui/logic/*.c`, include dir `ui/` + `ui/logic/`, dan:

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE LV_USE_XML=0)
spiffs_create_partition_image(storage ".../spiffs_assets" FLASH_IN_PROJECT)
```

`asset_fs.c` mem-cache tiap TTF ke PSRAM sekali. Tanpa itu, latency per-seek SPIFFS × 50 font × ratusan seek per font bikin watchdog trip saat boot. Jangan diganti `CONFIG_LV_USE_FS_STDIO`.

## 6. Config board yang wajib ikut

Semua ini hasil bring-up nyata di board fisik — kalau dilepas, gejalanya bukan error yang jelas:

| Setting | Kalau tidak dipakai |
| --- | --- |
| `CONFIG_LV_USE_CLIB_MALLOC=y` | **layar putih**: 50 tiny_ttf menghabiskan pool `LV_MEM` 64 KB |
| `CONFIG_LV_USE_LOG=y` + `LV_LOG_PRINTF` | assert LVGL jadi `while(1)` senyap, tanpa jejak |
| `task_stack = 32768` | `LoadStoreError` di `vPortYieldFromInt` (stb_truetype rakus stack) |
| `bounce_buffer_size_px` tetap terisi, `num_fbs = 1` | gambar **bergeser kumulatif** tiap repaint (DMA underrun baca PSRAM langsung) |
| touch `x_max/y_max = RES - 1` | 1 baris/kolom piksel tepi tidak bisa diklik (`x_max - x` overflow ke 480) |
| panel `mirror_x/mirror_y = true`, `sw_rotate = false` | layar terbalik 180° (board dipasang upside-down di enclosure) |

Bounce buffer dan multi-framebuffer **saling eksklusif** di board ini — `bb_fb_index` hanya menyusul `cur_fb_index` di batas frame, jadi 2 fb + bounce buffer menampilkan separuh frame lama. Sudah dicoba, sudah di-revert. Jangan diulang.

Sisa masalah kosmetik yang diketahui: glitch ringan saat transisi halaman. Arah perbaikan kalau mau dilanjut: tetap 1 fb + bounce buffer, lalu turunkan pixel clock 16 MHz atau naikkan `CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT` dari 20 untuk melebarkan margin bandwidth.

## 7. BSP: `^1.1.0`, bukan `^3.0.0`

Board fisik **rev 3.0** → BSP `^1.1.0` (IO expander **TCA9554**). BSP `^2`/`3.x` untuk HW **V4.0** (CH32V003 @ 0x24, penomoran EXIO beda). Salah pilih = driver expander salah.

```yaml
dependencies:
  waveshare/esp32_s3_touch_lcd_4: "^1.1.0"
  lvgl/lvgl: "^9.5.0"
  espressif/esp_lvgl_port: "^2.8.0"
```

BSP 1.1.0 **tidak compile di ESP-IDF v6** apa adanya: v6 menghapus `psram_trans_align` dan `bits_per_pixel` dari `esp_lcd_rgb_panel_config_t`, sementara manifest-nya klaim `idf: >=5.3` tanpa batas atas sehingga solver tidak menangkapnya. Solusi di repo ini: vendor komponennya ke `components/esp32_s3_touch_lcd_4/` (menimpa yang managed) dengan dua field itu dihapus — salinannya ada di `board/` dalam paket, sudah termasuk patch rotasi 180°, touch mirror, dan `touch_read_tolerant()`.

Patch terakhir itu wajib ikut: `esp_lvgl_port` membungkus `esp_lcd_touch_read_data()` dengan `ESP_ERROR_CHECK` (`esp_lvgl_port_touch.c:127`), jadi **satu** transaksi GT911 yang gagal di bus I²C bersama memanggil `abort()` dan mereboot board. Gejalanya menipu — layar hitam tapi backlight tetap menyala, karena backlight ada di TCA9554 yang tidak ikut reset. BSP di paket ini memasang read callback sendiri yang menahan state terakhir kalau read gagal.

## 8. Checklist serah-terima

Paket ini sudah diverifikasi lengkap: `cmake -S sim -B <dir> -DUI_DIR=handoff/triagebox-ui/ui` lalu build → link sukses, artinya tidak ada file C generated yang ketinggalan. Cara cepat cek ulang setelah export baru:

```sh
sh tools/make_handoff.sh                    # gagal kalau ada file di manifest yang hilang; juga bikin handoff/triagebox-ui.zip
cmake -S sim -B /tmp/pkgcheck -DUI_DIR=$PWD/handoff/triagebox-ui/ui && cmake --build /tmp/pkgcheck -j8
```

- [ ] `idf.py set-target esp32s3 && idf.py build` lolos
- [ ] Boot log: `TriageBox UI up on 480x480`, tanpa panic dan tanpa warning `indev_pointer_proc`
- [ ] `UI_MEASURE_MS` sudah 60000 untuk hardware
- [ ] `ui_mock.c` sudah diganti implementasi serial; `ui_mock.h` tidak berubah
- [ ] Tombol fisik dan touch memicu aksi yang sama di tiap layar (jalur `ui_action` tunggal)
