#ifndef UI_LOGIC_UI_SESSION_H
#define UI_LOGIC_UI_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_types.h"

void ui_session_reset(void);
void ui_session_new_scan(const rfid_t *rfid);

void ui_session_set_age(ui_age_band_t age);
void ui_session_set_gender(ui_gender_t gender);
/*
 * Airway obstruction, the third and only clinical judgement the operator makes.
 * The model treats it as RED on its own, so it is a question and not a default:
 * ui_session_has_airway() stays false until the Airway screen is answered.
 */
void ui_session_set_airway(bool problem);
/*
 * Respiratory rate as a counted band. Manual because the microphone is not on
 * the board, and load-bearing because tb_classify() refuses to score at all on
 * respiratory_rate <= 0 -- without this every real patient came out BLACK with
 * esi 0. ui_session_has_rr() stays false until the RR screen is answered.
 */
void ui_session_set_rr(ui_rr_band_t rr);
void ui_session_set_vitals(const vitals_t *vitals);
/*
 * The vitals THE VERDICT WAS COMPUTED FROM, latched once at measure-done.
 *
 * ui_session_set_vitals() keeps moving while the operator walks the
 * Result/Monitor pair (Monitor is a live screen, so it must). But Result is the
 * screen where the numbers beside the colour have to be the numbers that
 * PRODUCED that colour: the 50 ms poll blanks a tile the moment a finger leaves
 * the sensor, and after a Monitor visit the live copy no longer matches what the
 * model scored. This copy is written exactly once per measurement, right after
 * the classification pull in ui_runtime.c, and is what apply_result_vitals()
 * reads.
 */
void ui_session_set_measured_vitals(const vitals_t *vitals);
const vitals_t *ui_session_get_measured_vitals(void);
/*
 * esi is the model's raw 1..5 output, or 0 when it refused. Passed in the SAME
 * call as the priority on purpose: they are two views of one result, and separate
 * setters would allow a fresh colour next to a previous patient's ESI.
 */
void ui_session_set_priority(ui_priority_t priority, float confidence,
                             const char *reasons, int esi);
void ui_session_set_measurement_progress(uint8_t progress);

bool ui_session_has_rfid(void);
const rfid_t *ui_session_get_rfid(void);

bool ui_session_has_age(void);
ui_age_band_t ui_session_get_age(void);

bool ui_session_has_gender(void);
ui_gender_t ui_session_get_gender(void);

bool ui_session_has_airway(void);
bool ui_session_get_airway(void);

bool ui_session_has_rr(void);
ui_rr_band_t ui_session_get_rr(void);

const vitals_t *ui_session_get_vitals(void);

/* The priority value is meaningful only when ui_session_has_priority() is true. */
bool ui_session_has_priority(void);
ui_priority_t ui_session_get_priority(void);
float ui_session_get_confidence(void);
/* 1..5, or 0 for "the model refused to score". */
int ui_session_get_esi(void);
const char *ui_session_get_reasons(void);

uint8_t ui_session_get_measurement_progress(void);

#endif /* UI_LOGIC_UI_SESSION_H */
