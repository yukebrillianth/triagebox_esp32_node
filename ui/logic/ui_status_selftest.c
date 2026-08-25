#include "ui_status.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void test_sensors(void)
{
	assert(ui_status_sensors(UI_SENSOR_ALL) == UI_STATUS_OK);
	assert(ui_status_sensors(0) == UI_STATUS_ERROR);
	assert(ui_status_sensors(UI_SENSOR_MAX30102) == UI_STATUS_WARN);
	assert(ui_status_sensors(UI_SENSOR_ALL & ~UI_SENSOR_ECG) == UI_STATUS_WARN);
	/* Unknown high bits must not fake a healthy mask, in either direction. */
	assert(ui_status_sensors(0xE0U) == UI_STATUS_ERROR);
	assert(ui_status_sensors((uint8_t)(UI_SENSOR_ALL | 0xE0U)) == UI_STATUS_OK);
	/* The LoRa bit shares the byte but has its own dot: a radio-only report is
	 * still "no sensors", and a radio-less one is still "all sensors". This is
	 * the regression that once made an ECG dropout read as an RFID fault. */
	assert(ui_status_sensors(UI_SENSOR_LORA) == UI_STATUS_ERROR);
	assert(ui_status_sensors((uint8_t)(UI_SENSOR_ALL | UI_SENSOR_LORA)) == UI_STATUS_OK);
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

static void test_battery(void)
{
	char buf[8];

	assert(ui_status_battery_icon(0, false) == UI_BATTERY_ICON_EMPTY);
	assert(ui_status_battery_icon(24, false) == UI_BATTERY_ICON_EMPTY);
	assert(ui_status_battery_icon(25, false) == UI_BATTERY_ICON_MEDIUM);
	assert(ui_status_battery_icon(74, false) == UI_BATTERY_ICON_MEDIUM);
	assert(ui_status_battery_icon(75, false) == UI_BATTERY_ICON_FULL);
	assert(ui_status_battery_icon(100, false) == UI_BATTERY_ICON_FULL);

	assert(ui_status_battery_icon(0, true) == UI_BATTERY_ICON_CHARGING);
	assert(ui_status_battery_icon(100, true) == UI_BATTERY_ICON_CHARGING);

	/* An unread gauge must not draw a full battery. */
	assert(ui_status_battery_icon(UI_BATTERY_UNKNOWN, false) == UI_BATTERY_ICON_EMPTY);

	ui_status_battery_text(buf, sizeof(buf), 0);
	assert(strcmp(buf, "0%") == 0);
	ui_status_battery_text(buf, sizeof(buf), 100);
	assert(strcmp(buf, "100%") == 0);
	/* Flat and unread look identical if this fabricates a 0%. */
	ui_status_battery_text(buf, sizeof(buf), UI_BATTERY_UNKNOWN);
	assert(strcmp(buf, "--%") == 0);
	/* A decode fault clamps rather than printing "137%". */
	ui_status_battery_text(buf, sizeof(buf), 137);
	assert(strcmp(buf, "100%") == 0);

	ui_status_battery_text(NULL, sizeof(buf), 50);
	ui_status_battery_text(buf, 0, 50);
}

static void link(char *buf, unsigned sz, bool ok, bool reported, int dbm,
                 bool valid)
{
	ui_status_link_text(buf, sz, ok, reported, (int8_t)dbm, valid);
}

static void test_link_text(void)
{
	char a[UI_LINK_TEXT_MIN];
	char b[UI_LINK_TEXT_MIN];

	/* Nothing here may ever claim "Connected": the ESP32 cannot know a station
	 * heard us, only that the STM32 says its radio came up. */
	link(a, sizeof(a), true, true, 0, false);
	assert(strstr(a, "Connected") == NULL);

	/* No STM32 at all and STM32-present-radio-down are different faults. */
	link(a, sizeof(a), true, false, 0, false);
	link(b, sizeof(b), false, true, 0, false);
	assert(strcmp(a, b) != 0);
	link(b, sizeof(b), true, true, 0, false);
	assert(strcmp(a, b) != 0);
	link(a, sizeof(a), false, true, 0, false);
	assert(strcmp(a, b) != 0);

	/* A measured RSSI replaces the words: once there is a number, "LoRa siap"
	 * adds nothing, and the number is what someone range-testing reads. */
	link(a, sizeof(a), true, true, -97, true);
	assert(strcmp(a, "-97dBm") == 0);

	/* But a radio the STM32 reports DOWN still outranks a dBm from seconds ago:
	 * that is the state worth acting on. This is the ordering bug to catch. */
	link(a, sizeof(a), false, true, -97, true);
	assert(strcmp(a, "LoRa mati") == 0);
	/* And no STM32 at all outranks everything, since the RSSI came over it. */
	link(a, sizeof(a), true, false, -97, true);
	assert(strcmp(a, "Link --") == 0);

	/* The extremes must fit the buffer the header promises, NUL included. */
	link(a, sizeof(a), true, true, -128, true);
	assert(strcmp(a, "-128dBm") == 0);
	link(a, sizeof(a), true, true, -20, true);
	assert(strcmp(a, "-20dBm") == 0);

	/* Undersized buffer and NULL are ignored, not truncated into a half-number
	 * that reads as a different signal strength. */
	a[0] = 'x';
	ui_status_link_text(a, UI_LINK_TEXT_MIN - 1U, true, true, -97, true);
	assert(a[0] == 'x');
	ui_status_link_text(NULL, sizeof(a), true, true, -97, true);
}

static void test_rssi_state(void)
{
	/* Thresholds are the SX1278's, not a preference: ~-123 dBm sensitivity at
	 * SF7/125k, so -115 is a few dB from not decoding and -100 is comfortable. */
	assert(ui_status_rssi_state(-40) == UI_STATUS_OK);
	assert(ui_status_rssi_state(UI_RSSI_OK_DBM) == UI_STATUS_OK);
	assert(ui_status_rssi_state(UI_RSSI_OK_DBM - 1) == UI_STATUS_WARN);
	assert(ui_status_rssi_state(UI_RSSI_WARN_DBM) == UI_STATUS_WARN);
	assert(ui_status_rssi_state(UI_RSSI_WARN_DBM - 1) == UI_STATUS_ERROR);
	assert(ui_status_rssi_state(-128) == UI_STATUS_ERROR);

	/* Monotonic: a weaker signal is never reported as healthier than a stronger
	 * one. Stated as a property because the three-way compare above is exactly
	 * the shape that gets an inequality flipped in review. */
	for (int strong = -20; strong >= -127; strong--) {
		assert(ui_status_rssi_state((int8_t)strong) <=
		       ui_status_rssi_state((int8_t)(strong - 1)));
	}
}

static void test_clock(void)
{
	char buf[6];

	/* An unset ESP32 clock starts at the epoch; 07:00 on 1 Jan 1970 is the bug
	 * this guards. */
	ui_status_format_clock_at(buf, sizeof(buf), 0);
	assert(strcmp(buf, "--:--") == 0);
	ui_status_format_clock_at(buf, sizeof(buf), (time_t)(UI_CLOCK_VALID_EPOCH - 1));
	assert(strcmp(buf, "--:--") == 0);

	/* Once the RTC battery is fitted and the time set, it just works -- no code
	 * change, which is why the test is on the threshold and not on a flag. */
	ui_status_format_clock_at(buf, sizeof(buf), (time_t)UI_CLOCK_VALID_EPOCH);
	assert(strlen(buf) == 5);
	assert(isdigit((unsigned char)buf[0]));
	assert(isdigit((unsigned char)buf[1]));
	assert(buf[2] == ':');
	assert(isdigit((unsigned char)buf[3]));
	assert(isdigit((unsigned char)buf[4]));

	/* Too small to hold "--:--" is a no-op, not a truncated lie. */
	memset(buf, 'x', sizeof(buf));
	ui_status_format_clock_at(buf, 5, 0);
	assert(buf[0] == 'x');
	ui_status_format_clock_at(NULL, sizeof(buf), 0);
}

int main(void)
{
	test_battery();
	test_link_text();
	test_rssi_state();
	test_clock();
	test_sensors();
	test_system();
	test_lora();
	test_label();
	printf("ui_status_selftest: OK\n");
	return 0;
}
