#include "ui_status.h"

#include <stdio.h>
#include <time.h>

const char *ui_status_battery_icon(uint8_t percent, bool charging)
{
	if (charging) {
		return "battery_charging";
	}
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

/*
 * Silence longer than this means the STM32 is gone, not just idle. It must sit
 * above the slowest expected cadence or the dot flickers WARN forever: VITAL is
 * nominally 15 s and STATUS 30 s (AGENTS.md), so 45 s is one missed STATUS.
 */
#define LINK_STALE_MS 45000U

ui_status_state_t ui_status_sensors(uint8_t sensor_ok_mask)
{
	uint8_t known = (uint8_t)(sensor_ok_mask & UI_SENSOR_ALL);

	if (known == UI_SENSOR_ALL) {
		return UI_STATUS_OK;
	}
	return (known == 0U) ? UI_STATUS_ERROR : UI_STATUS_WARN;
}

ui_status_state_t ui_status_system(uint32_t age_ms, bool never_seen)
{
	if (never_seen) {
		return UI_STATUS_ERROR;
	}
	return (age_ms > LINK_STALE_MS) ? UI_STATUS_WARN : UI_STATUS_OK;
}

ui_status_state_t ui_status_lora(bool link_ok, bool reported)
{
	if (!reported) {
		return UI_STATUS_ERROR;
	}
	return link_ok ? UI_STATUS_OK : UI_STATUS_ERROR;
}

void ui_status_label(char *buf, unsigned buf_sz, const char *prefix,
                     ui_status_state_t state)
{
	const char *suffix;

	if (buf == NULL || buf_sz == 0 || prefix == NULL) {
		return;
	}
	switch (state) {
	case UI_STATUS_OK:   suffix = "OK"; break;
	case UI_STATUS_WARN: suffix = "!";  break;
	default:             suffix = "--"; break;
	}
	(void)snprintf(buf, buf_sz, "%s %s", prefix, suffix);
}
