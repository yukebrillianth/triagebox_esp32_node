#include "ui_status.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

int main(void)
{
	assert(strcmp(ui_status_battery_icon(0), "battery_empty") == 0);
	assert(strcmp(ui_status_battery_icon(24), "battery_empty") == 0);
	assert(strcmp(ui_status_battery_icon(25), "battery_medium") == 0);
	assert(strcmp(ui_status_battery_icon(74), "battery_medium") == 0);
	assert(strcmp(ui_status_battery_icon(75), "battery_full") == 0);
	assert(strcmp(ui_status_battery_icon(100), "battery_full") == 0);

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
