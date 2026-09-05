#include "ui_types.h"

const char *ui_priority_display_label(ui_priority_t value)
{
    switch (value) {
    case UI_PRIORITY_RED:
        return "MERAH - IMMEDIATE";
    case UI_PRIORITY_YELLOW:
        return "KUNING - DELAYED";
    case UI_PRIORITY_GREEN:
        return "HIJAU - MINOR";
    case UI_PRIORITY_BLACK:
        return "HITAM - EXPECTANT";
    }
    return "HIJAU - MINOR";
}

/*
 * "Refused to score" is esi < 1, not esi == 0: the ESI scale starts at 1, so
 * anything below it is outside the scale, and a corrupted negative deserves the
 * same honest "no score" as 0. Everything the Result screen does with a
 * refusal -- banner, label, icon, alarm -- keys off this predicate rather than
 * esi == 0 inline, so the sentinel has one home (see ui_types.h for why the
 * marker is the ESI and not a fifth priority).
 */
bool ui_verdict_unscored(int esi)
{
    return esi < 1;
}

const char *ui_verdict_label(ui_priority_t priority, int esi)
{
    if (ui_verdict_unscored(esi)) {
        return "TIDAK TERUKUR";
    }
    return ui_priority_display_label(priority);
}

/* Severity rank, low to high. The enum's own order is the Figma/wire order, not
 * this one -- see ui_priority_degraded() in ui_types.h. */
static int priority_rank(ui_priority_t value)
{
    switch (value) {
    case UI_PRIORITY_GREEN:
        return 0;
    case UI_PRIORITY_YELLOW:
        return 1;
    case UI_PRIORITY_RED:
        return 2;
    case UI_PRIORITY_BLACK:
        return 3;
    }
    return 0;
}

bool ui_priority_degraded(ui_priority_t from, ui_priority_t to)
{
    return priority_rank(to) > priority_rank(from);
}

uint16_t ui_rr_band_value(ui_rr_band_t band)
{
    switch (band) {
    case UI_RR_BAND_UNDER_12:
        return 10U;
    case UI_RR_BAND_12_20:
        return 16U;
    case UI_RR_BAND_21_30:
        return 25U;
    case UI_RR_BAND_OVER_30:
        return 36U;
    }
    /* Unreachable through ui_rr_band_t, but a cast from a stale integer would
     * land here, and 16 is the answer that skews the verdict least. */
    return 16U;
}
