# Hand-written logic only (Task 27 partial): prove link without full Pro lib-ui.
# ui_input.c needs LVGL indev; lvgl is already a managed dep via idf_component.yml.
idf_component_register(
    SRCS
        "app_main.c"
        "../ui/logic/ui_session.c"
        "../ui/logic/ui_mock.c"
        "../ui/logic/ui_nav.c"
        "../ui/logic/ui_action.c"
        "../ui/logic/ui_runtime.c"
        "../ui/logic/ui_input.c"
        "../ui/logic/ui_types.c"
        "../ui/logic/ui_status.c"
    INCLUDE_DIRS
        "."
        "../ui/logic"
    REQUIRES
        lvgl
)
