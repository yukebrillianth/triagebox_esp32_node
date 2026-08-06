#ifndef UI_LOGIC_UI_ICONS_H
#define UI_LOGIC_UI_ICONS_H

/*
 * Icon names are LVGL Pro <data name="..."> entries generated from Lucide (MIT)
 * PNGs under ui/images/icons/raw/ (see ui/globals.xml).
 *
 * Until Figma-export Lucide set is complete, map triage ButtonBar roles to the
 * closest existing Lucide glyph already converted in this project.
 * Prefer adding real Lucide PNGs (nfc, activity, droplets, gauge, …) over inventing art.
 */

/* ButtonBar roles */
#define UI_ICON_SCAN       "icon_search"      /* Lucide: search — replace with radio/nfc when added */
#define UI_ICON_POWER      "icon_power"       /* Lucide: power */
#define UI_ICON_MENU       "icon_menu"        /* Lucide: menu */
#define UI_ICON_START      "icon_play"        /* Lucide: play */
#define UI_ICON_RESTART    "icon_refresh"     /* Lucide: refresh-cw */
#define UI_ICON_ABORT      "icon_close"       /* Lucide: x */
#define UI_ICON_UP         "icon_chevron_up"  /* Lucide: chevron-up */
#define UI_ICON_DOWN       "icon_chevron_down"
#define UI_ICON_BACK       "icon_arrow_left"  /* Lucide: arrow-left */
#define UI_ICON_SELECT     "icon_check"       /* Lucide: check */
#define UI_ICON_MONITOR    "icon_activity"    /* fallback: icon_heart until activity added */
#define UI_ICON_RESET      "icon_refresh"
#define UI_ICON_STOP       "icon_pause"       /* Lucide: pause */

/* Status bar / vitals */
#define UI_ICON_BATTERY    "icon_battery_full"
#define UI_ICON_LINK       "icon_signal"      /* or icon_wifi */
#define UI_ICON_CLOCK      "icon_clock"
#define UI_ICON_HR         "icon_heart"       /* Lucide: heart */
#define UI_ICON_SPO2       "icon_droplets"    /* stand-in: icon_wifi until droplets added */
#define UI_ICON_RR         "icon_wind"        /* stand-in: icon_arrow_down until wind added */
#define UI_ICON_BP         "icon_gauge"       /* stand-in: icon_user until gauge added */
#define UI_ICON_WARNING    "icon_triangle_alert" /* stand-in: icon_bell */
#define UI_ICON_RFID       "icon_nfc"         /* not yet in raw/ — do not invent PNG */
#define UI_ICON_LOGO       "icon_home"        /* temporary until brand mark */

/* Concrete stand-ins that EXIST in this repo today (safe for XML right now): */
#undef UI_ICON_MONITOR
#define UI_ICON_MONITOR    "icon_heart"
#undef UI_ICON_SPO2
#define UI_ICON_SPO2       "icon_wifi"
#undef UI_ICON_RR
#define UI_ICON_RR         "icon_arrow_down"
#undef UI_ICON_BP
#define UI_ICON_BP         "icon_user"
#undef UI_ICON_WARNING
#define UI_ICON_WARNING    "icon_bell"
#undef UI_ICON_RFID
#define UI_ICON_RFID       "icon_bluetooth" /* closest existing until nfc PNG */
#undef UI_ICON_UP
#define UI_ICON_UP         "icon_arrow_up"
#undef UI_ICON_DOWN
#define UI_ICON_DOWN       "icon_arrow_down"

#endif /* UI_LOGIC_UI_ICONS_H */
