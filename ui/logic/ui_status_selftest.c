#include "ui_status.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

int main(void)
{
	assert(strcmp(ui_status_battery_icon(0, false), "battery_empty") == 0);
	assert(strcmp(ui_status_battery_icon(24, false), "battery_empty") == 0);
	assert(strcmp(ui_status_battery_icon(25, false), "battery_medium") == 0);
	assert(strcmp(ui_status_battery_icon(74, false), "battery_medium") == 0);
	assert(strcmp(ui_status_battery_icon(75, false), "battery_full") == 0);
	assert(strcmp(ui_status_battery_icon(100, false), "battery_full") == 0);

	assert(strcmp(ui_status_battery_icon(0, true), "battery_charging") == 0);
	assert(strcmp(ui_status_battery_icon(50, true), "battery_charging") == 0);
	assert(strcmp(ui_status_battery_icon(100, true), "battery_charging") == 0);

	char clock[6] = {0};
	ui_status_format_clock(clock, sizeof(clock));
	assert(strlen(clock) == 5);
	assert(isdigit((unsigned char)clock[0]));
	assert(isdigit((unsigned char)clock[1]));
	assert(clock[2] == ':');
	assert(isdigit((unsigned char)clock[3]));
	assert(isdigit((unsigned char)clock[4]));
	return 0;
}
