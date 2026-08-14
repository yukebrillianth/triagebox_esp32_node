#include "ui_status.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void test_sensors(void)
{
	assert(ui_status_sensors(UI_SENSOR_ALL) == UI_STATUS_OK);
	assert(ui_status_sensors(0) == UI_STATUS_ERROR);
	assert(ui_status_sensors(UI_SENSOR_HR) == UI_STATUS_WARN);
	assert(ui_status_sensors(UI_SENSOR_ALL & ~UI_SENSOR_ECG) == UI_STATUS_WARN);
	/* Unknown high bits must not fake a healthy mask, in either direction. */
	assert(ui_status_sensors(0xE0U) == UI_STATUS_ERROR);
	assert(ui_status_sensors((uint8_t)(UI_SENSOR_ALL | 0xE0U)) == UI_STATUS_OK);
}

static void test_system(void)
{
	/* Before the STM32 ever speaks, "OK" would be a lie. */
	assert(ui_status_system(0, true) == UI_STATUS_ERROR);
	assert(ui_status_system(100, false) == UI_STATUS_OK);
	/* Must tolerate the slowest normal cadence (STATUS every 30 s) without
	 * flickering; only a genuinely missed frame counts as stale. */
	assert(ui_status_system(30000, false) == UI_STATUS_OK);
	assert(ui_status_system(44999, false) == UI_STATUS_OK);
	assert(ui_status_system(45001, false) == UI_STATUS_WARN);
}

static void test_lora(void)
{
	/* Radio lives on the STM32: no report means unknown, and unknown is not OK. */
	assert(ui_status_lora(true, false) == UI_STATUS_ERROR);
	assert(ui_status_lora(false, true) == UI_STATUS_ERROR);
	assert(ui_status_lora(true, true) == UI_STATUS_OK);
}

static void test_label(void)
{
	char buf[24];

	ui_status_label(buf, sizeof(buf), "Sensor", UI_STATUS_OK);
	assert(strcmp(buf, "Sensor OK") == 0);
	ui_status_label(buf, sizeof(buf), "Sensor", UI_STATUS_WARN);
	assert(strcmp(buf, "Sensor !") == 0);
	ui_status_label(buf, sizeof(buf), "LoRa", UI_STATUS_ERROR);
	assert(strcmp(buf, "LoRa --") == 0);

	/* Bad args are no-ops, not crashes. */
	ui_status_label(NULL, sizeof(buf), "X", UI_STATUS_OK);
	ui_status_label(buf, 0, "X", UI_STATUS_OK);
	ui_status_label(buf, sizeof(buf), NULL, UI_STATUS_OK);
}

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

	test_sensors();
	test_system();
	test_lora();
	test_label();
	printf("ui_status_selftest: OK\n");
	return 0;
}
