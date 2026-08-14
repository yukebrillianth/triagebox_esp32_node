# Backlight + buzzer, both behind the TCA9554 expander. The sim links
# ui/logic/ui_board_stub.c instead of this component.
idf_component_register(
    SRCS
        "ui_board.c"
    INCLUDE_DIRS
        # ui_board.h is the contract; it lives with the other logic headers.
        "../../ui/logic"
    REQUIRES
        waveshare__esp32_s3_touch_lcd_4
        espressif__esp_io_expander
)
