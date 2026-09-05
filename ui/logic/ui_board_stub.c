/*
 * No-op ui_board.h for the desktop sim and the Editor preview: there is no
 * TCA9554 there. The device implementation is components/triagebox_board/.
 */
#include "ui_board.h"

void ui_board_init(void)
{
}

void ui_board_backlight(bool on)
{
    (void)on;
}

void ui_board_buzzer(bool on)
{
    (void)on;
}

void ui_board_power_off(void)
{
}

/* No PMIC here, so the status bar shows "--%" -- which is the honest answer. */
bool ui_board_battery(uint8_t *percent, bool *charging)
{
    (void)percent;
    (void)charging;
    return false;
}

bool ui_board_battery_mv(uint16_t *millivolts)
{
    (void)millivolts;
    return false;
}

bool ui_board_battery_ma(uint16_t *milliamps)
{
    (void)milliamps;
    return false;
}
