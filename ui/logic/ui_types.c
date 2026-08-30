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
