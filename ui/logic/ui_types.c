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
