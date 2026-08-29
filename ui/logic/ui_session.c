#include "ui_session.h"

#include <stddef.h>
#include <string.h>

#define UI_SESSION_REASONS_CAPACITY 128U

typedef struct {
    rfid_t rfid;
    ui_age_band_t age;
    bool has_age;
    ui_gender_t gender;
    bool has_gender;
    /*
     * Airway obstruction, answered by the operator on its own screen. Two fields
     * rather than a tri-state because "not asked yet" and "asked, answered no"
     * must not look alike: the model treats a set airway flag as RED on its own,
     * so an unanswered question defaulting to false is a silent downgrade.
     */
    bool airway_problem;
    bool has_airway;
    vitals_t vitals;
    ui_priority_t priority;
    bool has_priority;
    float confidence;
    /* The model's raw 1..5, or 0 when it refused. Set with the priority, never
     * on its own -- see ui_session_set_priority(). */
    int esi;
    char reasons[UI_SESSION_REASONS_CAPACITY];
    uint8_t measurement_progress;
} ui_session_state_t;

static ui_session_state_t session;

void ui_session_reset(void)
{
    memset(&session, 0, sizeof(session));
    session.age = UI_AGE_BAND_6_17;
    session.gender = UI_GENDER_U;
    session.priority = UI_PRIORITY_RED;
}

void ui_session_new_scan(const rfid_t *rfid)
{
    ui_session_reset();
    if (rfid != NULL) {
        session.rfid = *rfid;
        session.rfid.tag[RFID_TAG_CAPACITY - 1U] = '\0';
    }
}

void ui_session_set_age(ui_age_band_t age)
{
    session.age = age;
    session.has_age = true;
}

void ui_session_set_gender(ui_gender_t gender)
{
    session.gender = gender;
    session.has_gender = gender != UI_GENDER_U;
}

void ui_session_set_airway(bool problem)
{
    session.airway_problem = problem;
    session.has_airway = true;
}

void ui_session_set_vitals(const vitals_t *vitals)
{
    if (vitals != NULL) {
        session.vitals = *vitals;
    } else {
        memset(&session.vitals, 0, sizeof(session.vitals));
    }
}

void ui_session_set_priority(ui_priority_t priority, float confidence,
                             const char *reasons, int esi)
{
    size_t index = 0U;

    session.priority = priority;
    session.has_priority = true;
    session.confidence = confidence;
    session.esi = esi;

    if (reasons != NULL) {
        while (index + 1U < sizeof(session.reasons) && reasons[index] != '\0') {
            session.reasons[index] = reasons[index];
            ++index;
        }
    }
    session.reasons[index] = '\0';
}

void ui_session_set_measurement_progress(uint8_t progress)
{
    session.measurement_progress = progress > 100U ? 100U : progress;
}

bool ui_session_has_rfid(void)
{
    return session.rfid.present;
}

const rfid_t *ui_session_get_rfid(void)
{
    return &session.rfid;
}

bool ui_session_has_age(void)
{
    return session.has_age;
}

ui_age_band_t ui_session_get_age(void)
{
    return session.age;
}

bool ui_session_has_gender(void)
{
    return session.has_gender;
}

ui_gender_t ui_session_get_gender(void)
{
    return session.gender;
}

bool ui_session_has_airway(void)
{
    return session.has_airway;
}

bool ui_session_get_airway(void)
{
    return session.airway_problem;
}

const vitals_t *ui_session_get_vitals(void)
{
    return &session.vitals;
}

bool ui_session_has_priority(void)
{
    return session.has_priority;
}

ui_priority_t ui_session_get_priority(void)
{
    return session.priority;
}

int ui_session_get_esi(void)
{
    return session.esi;
}

float ui_session_get_confidence(void)
{
    return session.confidence;
}

const char *ui_session_get_reasons(void)
{
    return session.reasons;
}

uint8_t ui_session_get_measurement_progress(void)
{
    return session.measurement_progress;
}
