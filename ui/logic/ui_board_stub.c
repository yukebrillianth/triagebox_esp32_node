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
