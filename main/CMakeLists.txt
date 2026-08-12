# Exported LVGL Pro UI (screens/components/images/fonts C arrays + ui.c/ui_gen.c)
# come from the generated manifest. Hand-written logic + bindings are added on top.
set(LV_EDITOR_PROJECT_SOURCES)
include("${CMAKE_CURRENT_LIST_DIR}/../ui/file_list_gen.cmake")

idf_component_register(
    SRCS
        "app_main.c"
        "asset_fs.c"
        ${LV_EDITOR_PROJECT_SOURCES}
        "../ui/logic/ui_session.c"
        "../ui/logic/ui_mock.c"
        "../ui/logic/ui_nav.c"
        "../ui/logic/ui_action.c"
        "../ui/logic/ui_runtime.c"
        "../ui/logic/ui_input.c"
        "../ui/logic/ui_types.c"
        "../ui/logic/ui_status.c"
        "../ui/logic/ui_bindings.c"
    INCLUDE_DIRS
        "."
        "../ui"
        "../ui/logic"
    REQUIRES
        lvgl
        waveshare__esp32_s3_touch_lcd_4
        esp_lvgl_port
        spiffs
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE LV_USE_XML=0)

spiffs_create_partition_image(storage "${CMAKE_CURRENT_LIST_DIR}/../spiffs_assets" FLASH_IN_PROJECT)
