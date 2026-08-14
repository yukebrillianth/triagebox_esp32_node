/**
 * @file ui_gen.h
 */

#ifndef UI_GEN_H
#define UI_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
    #include "lvgl_private.h"
#else
    #include "lvgl/lvgl.h"
    #include "lvgl/lvgl_private.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif



/* Prototypes for target functions, needed by responsive const definitions */

void ui_set_target(uint32_t target);
uint32_t ui_get_target(void);
bool ui_check_target(uint32_t target);

/*********************
 *      DEFINES
 *********************/

#define UI_TARGET_UNDEFINED  (0 << 1)
#define UI_TARGET_TARGET1    (1 << 1)
#define UI_TARGET_ALL        0x0FFFFFFF

/* By default compile for all targets, allowing to switch to any targets at runtime */
#ifndef UI_COMPILE_TARGET
#define UI_COMPILE_TARGET UI_TARGET_ALL
#endif

#define UI_CHECK_COMPILE_TARGET(target) (UI_COMPILE_TARGET & (target) ? 1 : 0)

/**
 * Smallest spacing/padding unit
 */
#define SPACE_XS 2
/**
 * Small spacing/padding unit
 */
#define SPACE_SM 4
/**
 * Default spacing/padding unit
 */
#define SPACE_MD 8
/**
 * Large spacing/padding unit
 */
#define SPACE_LG 16
/**
 * Extra-large spacing/padding unit
 */
#define SPACE_XL 32
/**
 * Default corner radius (Figma card radius)
 */
#define RADIUS_DEFAULT 10
/**
 * Default border width
 */
#define BORDER_WIDTH 1
/**
 * Default icon size
 */
#define ICON_SIZE 16
/**
 * Used to dim down content
 */
#define OPA_MUTED (255 * 35 / 100)
/**
 * Light theme screen background
 */
#define COLOR_LIGHT_BG lv_color_hex(0xEEF1F6)
/**
 * Light theme panel/card background
 */
#define COLOR_LIGHT_PANEL lv_color_hex(0xFFFFFF)
/**
 * Light theme primary text
 */
#define COLOR_LIGHT_TEXT lv_color_hex(0x1B1F27)
/**
 * Dark theme screen background (Figma screen_bg)
 */
#define COLOR_DARK_BG lv_color_hex(0x0d1329)
/**
 * Dark theme panel/card background (Figma card_bg)
 */
#define COLOR_DARK_PANEL lv_color_hex(0x1a2651)
/**
 * Dark theme primary text
 */
#define COLOR_DARK_TEXT lv_color_hex(0xFFFFFF)
/**
 * Secondary / muted text
 */
#define COLOR_TEXT_SECONDARY lv_color_hex(0x99a1af)
/**
 * Text on dark card surfaces
 */
#define COLOR_TEXT_ON_CARD lv_color_hex(0xd1d5dc)
/**
 * Accent / active control color
 */
#define COLOR_ACCENT lv_color_hex(0x00d460)
/**
 * Text/icon on accent
 */
#define COLOR_ACCENT_TEXT lv_color_hex(0xFFFFFF)
/**
 * Destructive / Power / RED
 */
#define COLOR_DANGER lv_color_hex(0xfb2c36)
/**
 * Status OK indicator
 */
#define COLOR_STATUS_OK lv_color_hex(0x00c950)
/**
 * Status degraded indicator (sebagian sensor mati)
 */
#define COLOR_STATUS_WARN lv_color_hex(0xF0B100)
/**
 * Status failed indicator (tidak ada data / gagal)
 */
#define COLOR_STATUS_ERROR lv_color_hex(0xfb2c36)
/**
 * Neutral track
 */
#define COLOR_TRACK lv_color_hex(0x9AA3B2)
/**
 * ButtonBar cell gradient top (Figma #000827)
 */
#define COLOR_BUTTONBAR_TOP lv_color_hex(0x000827)
/**
 * ButtonBar cell gradient bottom (Figma #1a2651)
 */
#define COLOR_BUTTONBAR_BOTTOM lv_color_hex(0x1a2651)
/**
 * START YELLOW / DELAYED priority
 */
#define COLOR_PRIORITY_YELLOW lv_color_hex(0xF0B100)
/**
 * START GREEN / MINOR priority
 */
#define COLOR_PRIORITY_GREEN lv_color_hex(0x00C950)
/**
 * SpO2 / oxygen vital icon
 */
#define COLOR_VITAL_SPO2 lv_color_hex(0x51A2FF)
/**
 * HR / heart-pulse vital icon
 */
#define COLOR_VITAL_HR lv_color_hex(0xFB2C36)
/**
 * RR / respiratory vital icon
 */
#define COLOR_VITAL_RR lv_color_hex(0x53EAFD)
/**
 * BP / blood-pressure vital icon
 */
#define COLOR_VITAL_BP lv_color_hex(0xA78BFA)


#ifndef LV_XML_EVAL_STRING_BUF_SIZE
    #define LV_XML_EVAL_STRING_BUF_SIZE 256
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

extern lv_obj_t * age;
extern lv_obj_t * berhasil;
extern lv_obj_t * gender;
extern lv_obj_t * home;
extern lv_obj_t * mengukur;
extern lv_obj_t * monitor;
extern lv_obj_t * result;
extern lv_obj_t * scanning;

/*----------------
 * Global styles
 *----------------*/

extern lv_style_t shadow_button;
extern lv_style_t shadow_icon;
extern lv_style_t screen_base;
extern lv_style_t style_screen_light;
extern lv_style_t style_screen_dark;
extern lv_style_t style_panel_light;
extern lv_style_t style_panel_dark;
extern lv_style_t style_text_accent;
extern lv_style_t style_text_muted;
extern lv_style_t style_scrollbar;
extern lv_style_t style_buttonbar_cell_grad;

/*----------------
 * Fonts
 *----------------*/

/* Targets: any */
extern lv_font_t * font_inter_regular_36;
extern lv_font_t * font_inter_semi_bold_20;
extern lv_font_t * font_inter_semi_bold_16;
extern lv_font_t * font_inter_bold_32;
extern lv_font_t * font_inter_medium_16;
extern lv_font_t * font_inter_semi_bold_36;
extern lv_font_t * font_inter_bold_48;
extern lv_font_t * font_inter_semi_bold_18;
extern lv_font_t * font_inter_regular_14;
extern lv_font_t * font_inter_regular_13;
extern lv_font_t * font_inter_semi_bold_13;
extern lv_font_t * font_inter_regular_10;
extern lv_font_t * font_inter_regular_16;
extern lv_font_t * font_poppins_semibold_35_65217590332031;
extern lv_font_t * font_inter_semi_bold_24;
extern lv_font_t * font_inter_regular_12;
extern lv_font_t * font_inter_bold_24;
extern lv_font_t * font_inter_bold_20;
extern lv_font_t * font_inter_bold_18;
extern lv_font_t * font_inter_regular_9;
extern lv_font_t * font_inter_semi_bold_14;
extern lv_font_t * font_inter_bold_56;
extern lv_font_t * font_inter_regular_24;
extern lv_font_t * font_inter_semi_bold_12;
extern lv_font_t * font_inter_bold_36;
extern lv_font_t * font_inter_regular_11;
extern lv_font_t * font_inter_bold_28;
extern lv_font_t * font_inter_bold_47;
extern lv_font_t * font_inter_regular_47;
extern lv_font_t * font_helvetica_regular_7_000000476837158;
extern lv_font_t * font_inter_regular_12_000000953674316;
extern lv_font_t * font_inter_bold_30;
extern lv_font_t * font_inter_medium_14_000000953674316;
extern lv_font_t * font_inter_bold_14;
extern lv_font_t * font_inter_regular_15;
extern lv_font_t * font_inter_medium_13;
extern lv_font_t * font_inter_medium_12;
extern lv_font_t * font_inter_bold_12;
extern lv_font_t * font_inter_medium_15;
extern lv_font_t * font_inter_bold_14_000000953674316;
extern lv_font_t * font_inter_extra_bold_18;
extern lv_font_t * font_inter_medium_18;
extern lv_font_t * font_inter_medium_14;
extern lv_font_t * font_inter_regular_18;
extern lv_font_t * font_inter_bold_25;
extern lv_font_t * font_inter_medium_13_999999046325684;
extern lv_font_t * font_inter_extra_bold_20;
extern lv_font_t * font_inter_bold_35;
extern lv_font_t * font_inter_extra_bold_25;
extern lv_font_t * font_inter_medium_10;
extern lv_font_t * font_body_symbols;
extern lv_font_t * font_body;
extern lv_font_t * font_h5;
extern lv_font_t * font_h4;
extern lv_font_t * font_h3;
extern lv_font_t * font_h2;
extern lv_font_t * font_h1;


/*----------------
 * Images
 *----------------*/

/* Targets: any */
extern const void * icon_161221;
extern const void * icon_361488;
extern const void * icon_63636;
extern const void * icon_63640;
extern const void * icon_63653;
extern const void * icon_63657;
extern const void * vec_36_1610;
extern const void * vec_36_1611;
extern const void * vec_36_1612;
extern const void * vec_36_1613;
extern const void * vec_36_1614;
extern const void * vec_36_1618;
extern const void * vec_36_1619;
extern const void * vec_36_1620;
extern const void * vec_36_1621;
extern const void * vec_36_1622;
extern const void * vec_60_215;
extern const void * vec_60_216;
extern const void * vec_60_217;
extern const void * vec_60_218;
extern const void * vec_60_219;
extern const void * vec_60_224;
extern const void * vec_60_225;
extern const void * vec_60_230;
extern const void * vec_60_231;
extern const void * vec_60_232;
extern const void * vec_282_31;
extern const void * vec_282_32;
extern const void * vec_282_33;
extern const void * vec_282_35;
extern const void * vec_16_385;
extern const void * vec_16_386;
extern const void * vec_16_387;
extern const void * vec_16_388;
extern const void * vec_16_389;
extern const void * vec_36_1588;
extern const void * vec_36_1589;
extern const void * vec_36_1590;
extern const void * vec_36_1591;
extern const void * vec_36_1592;
extern const void * vec_36_1596;
extern const void * vec_36_1597;
extern const void * vec_36_1598;
extern const void * vec_36_1599;
extern const void * vec_36_1600;
extern const void * vec_60_271;
extern const void * vec_60_272;
extern const void * vec_60_278;
extern const void * vec_60_279;
extern const void * vec_60_284;
extern const void * vec_60_285;
extern const void * vec_60_286;
extern const void * vec_16_583;
extern const void * vec_16_584;
extern const void * vec_16_585;
extern const void * vec_16_586;
extern const void * vec_16_587;
extern const void * vec_36_1566;
extern const void * vec_36_1567;
extern const void * vec_36_1568;
extern const void * vec_36_1569;
extern const void * vec_36_1570;
extern const void * vec_36_1574;
extern const void * vec_36_1575;
extern const void * vec_36_1576;
extern const void * vec_36_1577;
extern const void * vec_36_1578;
extern const void * vec_60_145;
extern const void * vec_60_150;
extern const void * vec_60_151;
extern const void * vec_60_156;
extern const void * vec_60_157;
extern const void * vec_60_162;
extern const void * vec_60_163;
extern const void * vec_60_164;
extern const void * vec_16_1205;
extern const void * vec_16_1206;
extern const void * vec_16_1207;
extern const void * vec_68_1023;
extern const void * vec_68_1024;
extern const void * vec_16_1235;
extern const void * vec_16_1236;
extern const void * vec_16_1237;
extern const void * vec_68_865;
extern const void * vec_68_866;
extern const void * vec_36_1544;
extern const void * vec_36_1545;
extern const void * vec_36_1546;
extern const void * vec_36_1547;
extern const void * vec_36_1548;
extern const void * vec_36_1552;
extern const void * vec_36_1553;
extern const void * vec_36_1554;
extern const void * vec_36_1555;
extern const void * vec_36_1556;
extern const void * vec_63_375;
extern const void * vec_63_376;
extern const void * vec_60_124;
extern const void * vec_60_125;
extern const void * vec_60_130;
extern const void * vec_60_131;
extern const void * vec_60_136;
extern const void * vec_60_137;
extern const void * vec_60_138;
extern const void * vec_36_1497;
extern const void * vec_36_1498;
extern const void * vec_36_1499;
extern const void * vec_68_1019;
extern const void * vec_68_1020;
extern const void * vec_68_1007;
extern const void * vec_68_1008;
extern const void * vec_36_1522;
extern const void * vec_36_1523;
extern const void * vec_36_1524;
extern const void * vec_36_1525;
extern const void * vec_36_1526;
extern const void * vec_36_1530;
extern const void * vec_36_1531;
extern const void * vec_36_1532;
extern const void * vec_36_1533;
extern const void * vec_36_1534;
extern const void * vec_63_369;
extern const void * vec_63_370;
extern const void * vec_60_104;
extern const void * vec_60_105;
extern const void * vec_60_110;
extern const void * vec_60_111;
extern const void * vec_60_112;
extern const void * vec_60_3;
extern const void * vec_60_12;
extern const void * vec_60_82;
extern const void * vec_60_83;
extern const void * vec_60_88;
extern const void * vec_56_1838;
extern const void * vec_56_1839;
extern const void * vec_56_1840;
extern const void * vec_56_1841;
extern const void * vec_56_1842;
extern const void * vec_56_1846;
extern const void * vec_56_1847;
extern const void * vec_56_1848;
extern const void * vec_56_1849;
extern const void * vec_56_1850;
extern const void * vec_60_294;
extern const void * vec_60_299;
extern const void * vec_60_304;
extern const void * vec_60_305;
extern const void * vec_60_310;
extern const void * vec_60_317;
extern const void * vec_60_318;
extern const void * vec_60_319;
extern const void * vec_60_320;
extern const void * vec_60_321;
extern const void * vec_60_325;
extern const void * vec_60_326;
extern const void * vec_60_327;
extern const void * vec_60_328;
extern const void * vec_60_329;
extern const void * vec_60_348;
extern const void * vec_60_349;
extern const void * vec_60_350;
extern const void * vec_63_363;
extern const void * vec_63_364;
extern const void * vec_63_365;
extern const void * vec_63_418;
extern const void * vec_63_419;
extern const void * vec_63_420;
extern const void * vec_63_421;
extern const void * vec_63_422;
extern const void * vec_63_426;
extern const void * vec_63_427;
extern const void * vec_63_428;
extern const void * vec_63_429;
extern const void * vec_63_430;
extern const void * vec_63_834;
extern const void * vec_63_835;
extern const void * vec_63_840;
extern const void * vec_63_841;
extern const void * vec_63_450;
extern const void * vec_63_451;
extern const void * vec_63_456;
extern const void * vec_63_457;
extern const void * vec_63_458;
extern const void * vec_63_667;
extern const void * vec_63_668;
extern const void * vec_63_669;
extern const void * vec_63_673;
extern const void * vec_63_674;
extern const void * vec_63_687;
extern const void * vec_63_688;
extern const void * vec_63_689;
extern const void * icon_arrow_left;
extern const void * icon_check;
extern const void * icon_chevron_down;
extern const void * icon_chevron_up;
extern const void * icon_close;
extern const void * icon_gender_male;
extern const void * icon_gender_female;
extern const void * icon_monitor;
extern const void * icon_priority_immediate;
extern const void * icon_priority_delayed;
extern const void * icon_priority_minor;
extern const void * icon_vital_spo2_sm;
extern const void * icon_vital_hr_sm;
extern const void * icon_vital_rr_sm;
extern const void * icon_vital_bp_sm;
extern const void * icon_update;
extern const void * icon_heart_pulse;
extern const void * icon_oxygen;
extern const void * icon_respiratory;
extern const void * icon_blood_pressure;
extern const void * icon_menu;
extern const void * icon_pause;
extern const void * icon_play;
extern const void * icon_power;
extern const void * icon_refresh;
extern const void * icon_search;
extern const void * icon_rfid_scan;
extern const void * icon_rfid_scan_lg;
extern const void * icon_signal;
extern const void * icon_star;
extern const void * icon_wifi_high;
extern const void * icon_wifi_low;
extern const void * icon_wifi_zero;
extern const void * logo_light_for_dark;
extern const void * battery_empty;
extern const void * battery_medium;
extern const void * battery_full;
extern const void * battery_charging;

/*----------------
 * Subjects
 *----------------*/

extern lv_subject_t subject_theme_dark;
extern lv_subject_t subject_brightness;
extern lv_subject_t subject_show_keyboard;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

/**
 * Initialize the component library
 */

void ui_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widgets, components and screens of this library*/
#include "components/controls/arc/arc_gen.h"
#include "components/controls/bar/bar_gen.h"
#include "components/controls/button/button_gen.h"
#include "components/controls/checkbox/checkbox_gen.h"
#include "components/controls/dropdown/dropdown_gen.h"
#include "components/controls/keyboard/keyboard_gen.h"
#include "components/controls/slider/slider_gen.h"
#include "components/controls/switch/switch_gen.h"
#include "components/controls/text_box/text_box_gen.h"
#include "components/controls/text_input/text_input_gen.h"
#include "components/images/image/image_gen.h"
#include "components/images/monoicon/monoicon_gen.h"
#include "components/layout/base_box/base_box_gen.h"
#include "components/layout/column/column_gen.h"
#include "components/layout/container/container_gen.h"
#include "components/layout/panel/panel_gen.h"
#include "components/layout/row/row_gen.h"
#include "components/list/list_item/list_item_gen.h"
#include "components/list/list_section/list_section_gen.h"
#include "components/list/list_separator/list_separator_gen.h"
#include "components/list/list/list_gen.h"
#include "components/triage/button_bar_gen.h"
#include "components/triage/result_vital_gen.h"
#include "components/triage/status_bar_gen.h"
#include "components/triage/vital_card_gen.h"
#include "components/typography/h1/h1_gen.h"
#include "components/typography/h2/h2_gen.h"
#include "components/typography/h3/h3_gen.h"
#include "components/typography/h4/h4_gen.h"
#include "components/typography/h5/h5_gen.h"
#include "components/typography/text/text_gen.h"
#include "screens/age_gen.h"
#include "screens/berhasil_gen.h"
#include "screens/gender_gen.h"
#include "screens/home_gen.h"
#include "screens/mengukur_gen.h"
#include "screens/monitor_gen.h"
#include "screens/result_gen.h"
#include "screens/scanning_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*UI_GEN_H*/