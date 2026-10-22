# Backlight + buzzer behind the TCA9554 expander, plus power-off via the SW6106
# PMIC on the same bus. The sim links ui/logic/ui_board_stub.c instead.
idf_component_register(
    SRCS
        "ui_board.c"
        "tb_rtc.c"
    INCLUDE_DIRS
        # ui_board.h is the contract; it lives with the other logic headers.
        "../../ui/logic"
        "."  # tb_rtc.h: app_main sets the clock at boot, tb_debug has `rtc`
    REQUIRES
        waveshare__esp32_s3_touch_lcd_4
        espressif__esp_io_expander
        # The PMIC is addressed directly on the BSP's bus, not via the expander.
        esp_driver_i2c
)
