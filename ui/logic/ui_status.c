#include "ui_status.h"

#include <stdio.h>
#include <time.h>

const char *ui_status_battery_icon(uint8_t percent)
{
	if (percent < 25) {
		return "battery_empty";
	}
	if (percent < 75) {
		return "battery_medium";
	}
	return "battery_full";
}

void ui_status_format_clock(char *buf, unsigned buf_sz)
{
	if (buf == NULL || buf_sz < 6) {
		return;
	}

	time_t now = time(NULL);
	struct tm *local = localtime(&now);
	if (local == NULL) {
		(void)snprintf(buf, buf_sz, "--:--");
		return;
	}

	(void)snprintf(buf, buf_sz, "%02d:%02d", local->tm_hour, local->tm_min);
}
