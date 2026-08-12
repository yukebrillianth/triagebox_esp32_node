#ifndef UI_STATUS_H
#define UI_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *ui_status_battery_icon(uint8_t percent, bool charging);
void ui_status_format_clock(char *buf, unsigned buf_sz);

#ifdef __cplusplus
}
#endif

#endif
