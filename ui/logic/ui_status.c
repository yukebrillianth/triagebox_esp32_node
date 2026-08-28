#include "ui_status.h"

#include <stdio.h>
#include <time.h>

void ui_status_link_text(char *buf, unsigned buf_sz, bool lora_ok, bool reported,
                         int8_t rssi_dbm, bool rssi_valid)
{
	if (buf == NULL || buf_sz < UI_LINK_TEXT_MIN) {
		return;
	}
	/* Most actionable first. See the header for why the number wins over the
	 * words once there is one, and why "LoRa mati" still outranks it. */
	if (!reported) {
		/* "--" for unknown matches the rest of the bar (battery "--%", clock
		 * "--:--", ui_status_label's ERROR suffix) rather than inventing a
		 * third vocabulary for the same idea. */
		(void)snprintf(buf, buf_sz, "Link --");
	} else if (!lora_ok) {
		(void)snprintf(buf, buf_sz, "LoRa mati");
	} else if (rssi_valid) {
		(void)snprintf(buf, buf_sz, "%ddBm", (int)rssi_dbm);
	} else {
		(void)snprintf(buf, buf_sz, "LoRa siap");
	}
}

ui_status_state_t ui_status_rssi_state(int8_t dbm)
{
	if (dbm >= UI_RSSI_OK_DBM) {
		return UI_STATUS_OK;
	}
	return (dbm >= UI_RSSI_WARN_DBM) ? UI_STATUS_WARN : UI_STATUS_ERROR;
}

void ui_status_battery_text(char *buf, unsigned buf_sz, uint8_t percent)
{
	if (buf == NULL || buf_sz < 5) {
		return;
	}
	if (percent == UI_BATTERY_UNKNOWN) {
		(void)snprintf(buf, buf_sz, "--%%");
		return;
	}
	/* The gauge is 0-100; anything else is a decode fault, and clamping is
	 * kinder than rendering "137%" on a triage screen. */
	if (percent > 100U) {
		percent = 100U;
	}
	(void)snprintf(buf, buf_sz, "%u%%", (unsigned)percent);
}

ui_battery_icon_t ui_status_battery_icon(uint8_t percent, bool charging)
{
	if (charging) {
		return UI_BATTERY_ICON_CHARGING;
	}
	if (percent == UI_BATTERY_UNKNOWN || percent < 25U) {
		return UI_BATTERY_ICON_EMPTY;
	}
	if (percent < 75U) {
		return UI_BATTERY_ICON_MEDIUM;
	}
	return UI_BATTERY_ICON_FULL;
}

void ui_status_format_clock_at(char *buf, unsigned buf_sz, time_t now)
{
	struct tm *local;

	if (buf == NULL || buf_sz < 6) {
		return;
	}
	if (now < (time_t)UI_CLOCK_VALID_EPOCH) {
		(void)snprintf(buf, buf_sz, "--:--");
		return;
	}

	local = localtime(&now);
	if (local == NULL) {
		(void)snprintf(buf, buf_sz, "--:--");
		return;
	}

	(void)snprintf(buf, buf_sz, "%02d:%02d", local->tm_hour, local->tm_min);
}

void ui_status_format_clock(char *buf, unsigned buf_sz)
{
	ui_status_format_clock_at(buf, buf_sz, time(NULL));
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
