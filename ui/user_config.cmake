# =============================================================================
# user_config.cmake - Add your custom source files here
# =============================================================================
#
# This file is included by the generated CMakeLists.txt and allows you to
# add extra source files to the project without modifying generated files
# (which may be overwritten).
#
# To add your own sources, append them to LV_EDITOR_PROJECT_SOURCES:
#
#   list(APPEND LV_EDITOR_PROJECT_SOURCES
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_widget.c
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_screen.c
#   )
#
# Tip:
#   - Use ${CMAKE_CURRENT_LIST_DIR} to get paths relative to this file
#
# =============================================================================

list(APPEND LV_EDITOR_PROJECT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_bindings.c
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_action.c
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_nav.c
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_session.c
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_types.c
    # ui_bindings.c reads the Home status dots through ui_status/ui_mock, so the
    # Editor preview has to link them too or Ctrl+B fails at wasm-ld.
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_status.c
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_mock.c
    # Demo mode flag + fake vitals, read by ui_bindings' Menu dialog.
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_demo.c
    # The Airway screen, built in C rather than authored in the Editor -- see
    # ui_airway.h. ui_bindings.c calls it, so the preview has to link it too.
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_airway.c
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_rr.c
    # No expander in the Editor preview -- link the no-op board.
    ${CMAKE_CURRENT_LIST_DIR}/logic/ui_board_stub.c
)
