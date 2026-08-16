# RS485 link to the STM32. tb_frame.c is platform-neutral (the STM32 side can
# reuse it); tb_link.c is ESP-IDF. Selftests are host-only — see
# tools/run_selftests.sh — so they are not listed here.
idf_component_register(
    SRCS
        "tb_frame.c"
        "tb_link.c"
        "tb_ui_source.c"
    INCLUDE_DIRS
        "include"
        # ui_types.h / ui_mock.h: the contract this component implements.
        "../../ui/logic"
    REQUIRES
        # IDF v6 split the old monolithic "driver" component; UART lives here.
        esp_driver_uart
        triagebox_ml
        # ui_mock_power_off() cuts the rail via ui_board_power_off().
        triagebox_board
)
