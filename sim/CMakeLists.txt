cmake_minimum_required(VERSION 3.18)
project(lvgl_simulator C)

# Build with debug symbols by default (so breakpoints work) unless the user
# picked a build type. Multi-config generators (Visual Studio) ignore this and
# choose the config at build time instead.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
#  Options
# ---------------------------------------------------------------------------
# LVGL version to fetch (any git tag or branch from https://github.com/lvgl/lvgl)
set(LVGL_VERSION "v9.5.0" CACHE STRING "LVGL version (git tag or branch)")

# Folder containing the C code exported from the LVGL Editor.
# The Editor project lives in the sibling ../ui directory (this CMakeLists is in sim/).
set(UI_DIR "${CMAKE_SOURCE_DIR}/../ui" CACHE PATH "Folder with the UI exported from the LVGL Editor")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------------------
#  Fetch LVGL
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG ${LVGL_VERSION}
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR "skip_add_subdirectory")

FetchContent_GetProperties(lvgl)
FetchContent_MakeAvailable(lvgl)

# ---------------------------------------------------------------------------
#  Generate lv_conf.h from the platform's lv_conf defaults
#
#  Each lv_conf_*.defaults only lists the options we override; every other
#  option is taken from LVGL's own lv_conf_template.h. The display/input
#  backend is baked into the per-platform defaults file we pick here.
# ---------------------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(WIN32)
  # On Windows use LVGL's built-in Win32 driver (no external dependency).
  set(LV_CONF_DEFAULTS "${CMAKE_SOURCE_DIR}/lv_conf_windows.defaults")
else()
  # On Linux and macOS use SDL2.
  set(LV_CONF_DEFAULTS "${CMAKE_SOURCE_DIR}/lv_conf_sdl.defaults")
endif()

set(LV_CONF_PATH "${CMAKE_BINARY_DIR}/lv_conf.h")
# Generate to a temp file first, then copy only when the contents changed so
# lv_conf.h keeps its timestamp across reconfigures and LVGL is not rebuilt
# on every build.
set(LV_CONF_TMP "${CMAKE_BINARY_DIR}/lv_conf.h.in")
execute_process(
  COMMAND ${Python3_EXECUTABLE}
          "${lvgl_SOURCE_DIR}/scripts/generate_lv_conf.py"
          --template "${lvgl_SOURCE_DIR}/lv_conf_template.h"
          --defaults "${LV_CONF_DEFAULTS}"
          --config   "${LV_CONF_TMP}"
  RESULT_VARIABLE _conf_result
  OUTPUT_VARIABLE _conf_output
  ERROR_VARIABLE  _conf_output)
if(NOT _conf_result EQUAL 0)
  message(FATAL_ERROR "Failed to generate lv_conf.h:\n${_conf_output}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LV_CONF_TMP}" "${LV_CONF_PATH}")
message(STATUS "Generated ${LV_CONF_PATH} from ${LV_CONF_DEFAULTS}")

# Re-run CMake (and regenerate lv_conf.h) when the defaults file changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${LV_CONF_DEFAULTS}")

# Tell LVGL to use the generated lv_conf.h.
set(LV_BUILD_CONF_PATH "${LV_CONF_PATH}" CACHE PATH "" FORCE)

# ---------------------------------------------------------------------------
#  Add LVGL (keep the build minimal: skip bundled demos/examples/ThorVG)
# ---------------------------------------------------------------------------
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

add_subdirectory(${lvgl_SOURCE_DIR} ${lvgl_BINARY_DIR})

# ---------------------------------------------------------------------------
#  Display / input backend
# ---------------------------------------------------------------------------
set(BACKEND_LIBS "")
if(WIN32)
  # LVGL's Windows driver only needs the Win32 GDI libraries.
  list(APPEND BACKEND_LIBS gdi32 user32)
else()
  # SDL2 provides the window, mouse and keyboard on Linux/macOS.
  find_package(SDL2 REQUIRED)
  target_include_directories(lvgl SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
  list(APPEND BACKEND_LIBS ${SDL2_LIBRARIES})
endif()

# ---------------------------------------------------------------------------
#  UI exported from the LVGL Editor (optional)
#
#  When you export C code into the ui/ folder it ships its own CMakeLists.txt
#  that builds a `lib-ui` target. We pick it up automatically.
# ---------------------------------------------------------------------------
set(UI_LIB "")
if(EXISTS "${UI_DIR}/CMakeLists.txt" AND EXISTS "${UI_DIR}/ui.h")
  message(STATUS "Found exported UI project in: ${UI_DIR}")
  add_subdirectory(${UI_DIR} ${CMAKE_BINARY_DIR}/ui)
  set(UI_LIB lib-ui)
else()
  message(STATUS "Complete exported UI not found; building the welcome screen")
endif()

# ---------------------------------------------------------------------------
#  Application
# ---------------------------------------------------------------------------
add_executable(${CMAKE_PROJECT_NAME} src/main.c src/hal.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/..")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE lvgl ${UI_LIB} ${BACKEND_LIBS})

if(UI_LIB)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAS_UI=1)
endif()

if(NOT WIN32)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m pthread)
endif()

# Build and run from the repository root so the "A:ui" asset path resolves to ui/.
add_custom_target(run
  COMMAND $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
  DEPENDS ${CMAKE_PROJECT_NAME})
