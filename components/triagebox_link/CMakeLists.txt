# Link to the STM32F411 slave.
#
# TRANSPORT IS CHOSEN HERE, NOT BY #ifdef. tb_link_i2c.c (I2C master, current)
# and tb_link.c (RS485/UART, superseded) implement the same tb_link_* surface,
# so exactly one may be listed. The RS485 side was never implemented on the
# STM32 -- that project has no USART at all -- so I2C is the only transport that
# has ever had two ends. tb_link.c is kept for one release in case the swap has
# to be reverted; delete it once the I2C link has run on hardware.
#
# tb_frame.c stays regardless: the LoRa payload still uses its priority
# conversion, and tb_i2c_codec.c/tb_frame.c are both platform-neutral and host-
# tested (see tools/run_selftests.sh, which is why no selftest is listed here).
idf_component_register(
    SRCS
        "tb_frame.c"
        "tb_i2c_codec.c"
        "tb_link_i2c.c"
        "tb_ui_source.c"
    INCLUDE_DIRS
        "include"
        # ui_types.h / ui_mock.h: the contract this component implements.
        "../../ui/logic"
    REQUIRES
        # IDF v6 split the old monolithic "driver" component; the new
        # i2c_master.h API lives here.
        esp_driver_i2c
        # bsp_i2c_get_handle(): the STM32 shares the display's I2C bus.
        esp32_s3_touch_lcd_4
        triagebox_ml
        # ui_mock_power_off() cuts the rail via ui_board_power_off().
        triagebox_board
)
