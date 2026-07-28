# LVGL Simulator for VSCode

A minimal, ready-to-debug PC simulator for [LVGL](https://github.com/lvgl/lvgl)
and the [LVGL Editor](https://lvgl.io/pro). It runs your UI on your computer in
an SDL window (or a native window on Windows) so you can build and debug
embedded GUIs without any hardware. LVGL itself is downloaded automatically by
CMake, the configuration is generated from a small `lv_conf_*.defaults` file, and
the `.vscode/` folder is pre-configured so you can compile and debug with a
single key press. Export your UI from the LVGL Editor into the `ui/` folder and
it is picked up automatically.

## Dependencies

You need **CMake** (≥ 3.16), **Python 3** (used by LVGL to generate
`lv_conf.h`), a **C compiler**, and a **debugger**. On Linux and macOS you also
need **SDL2**; on Windows nothing extra is required because LVGL's built-in
Win32 driver is used instead.

### Linux

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake gdb python3 libsdl2-dev
# Arch
sudo pacman -S base-devel cmake gdb python sdl2
# Fedora
sudo dnf install @development-tools cmake gdb python3 SDL2-devel
```

### macOS

```bash
brew install cmake llvm python sdl2
```

### Windows

Install [CMake](https://cmake.org/download/),
[Python 3](https://www.python.org/downloads/) and a toolchain that ships
`gcc`/`gdb` ([MSYS2](https://www.msys2.org/) → `pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake`).
No SDL installation is needed — the native LVGL Windows driver is used.

## Configuration options

LVGL is configured through a per-platform defaults file: **`lv_conf_sdl.defaults`**
on Linux/macOS and **`lv_conf_windows.defaults`** on Windows. CMake picks the
right one automatically. Each only lists the options that differ from LVGL's
defaults; CMake merges it with LVGL's `lv_conf_template.h` to produce
`build/lv_conf.h`. Edit the file for your platform and re-build to change things
such as:

- `LV_COLOR_DEPTH` — color depth (default `32`).
- `LV_USE_PERF_MONITOR` — set to `1` for an on-screen FPS/CPU overlay.
- `LV_FONT_MONTSERRAT_*` — enable additional built-in fonts.

CMake cache options (pass with `-D`):

- `LVGL_VERSION` — LVGL git tag/branch to fetch (default `v9.5.0`).
- `UI_DIR` — folder with the exported Editor code (default `ui`).

The display/input backend (SDL vs. the Windows driver) is selected
automatically per platform, so you normally don't touch it.

## How to build

### In VSCode (recommended)

1. Install the recommended extensions when prompted (_CMake Tools_, _C/C++_).
2. CMake configures automatically on open (LVGL is downloaded on first run).
3. Press **F5** to build and start debugging.

### From the command line

```bash
cmake -B build
cmake --build build
./build/bin/lvgl_simulator
```

> Run the binary from the **project root** so that the `A:ui` asset path
> resolves to the `ui/` folder. The bundled `run` target does this for you:
> `cmake --build build --target run`.

## Using your LVGL Editor UI

Export C code from the LVGL Editor into the [`ui/`](ui/) folder. It is detected
automatically; `main.c` then calls `ui_init("A:ui")`. Load one of your screens
by uncommenting and editing the `lv_screen_load(...)` line in
[`src/main.c`](src/main.c). See the LVGL docs on
[using exported C code](https://lvgl.io/docs/pro/integration/using-exported-c-code).

## Support

- LVGL Open documentation: [https://lvgl.io/docs/open](https://lvgl.io/docs/open)
- LVGL Pro documentation: [https://lvgl.io/docs/pro](https://lvgl.io/docs/pro)
- LVGL forum: [https://forum.lvgl.io](https://forum.lvgl.io)
- Report issues for this template: [https://github.com/lvgl/lvgl_editor/issues](https://github.com/lvgl/lvgl_editor/issues)
- Report issues for LVGL: [https://github.com/lvgl/lvgl/issues](https://github.com/lvgl/lvgl/issues)
