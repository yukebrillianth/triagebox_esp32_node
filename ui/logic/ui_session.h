#ifndef UI_LOGIC_UI_SESSION_H
#define UI_LOGIC_UI_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_types.h"

void ui_session_reset(void);
void ui_session_new_scan(const rfid_t *rfid);

void ui_session_set_age(ui_age_band_t age);
void ui_session_set_gender(ui_gender_t gender);
void ui_session_set_vitals(const vitals_t *vitals);
void ui_session_set_priority(ui_priority_t priority, float confidence, const char *reasons);
void ui_session_set_measurement_progress(uint8_t progress);

bool ui_session_has_rfid(void);
const rfid_t *ui_session_get_rfid(void);

bool ui_session_has_age(void);
ui_age_band_t ui_session_get_age(void);

bool ui_session_has_gender(void);
ui_gender_t ui_session_get_gender(void);

const vitals_t *ui_session_get_vitals(void);

/* The priority value is meaningful only when ui_session_has_priority() is true. */
bool ui_session_has_priority(void);
ui_priority_t ui_session_get_priority(void);
float ui_session_get_confidence(void);
const char *ui_session_get_reasons(void);

uint8_t ui_session_get_measurement_progress(void);

#endif /* UI_LOGIC_UI_SESSION_H */
