/**
 * @file ui_gen.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "ui_gen.h"

#if defined(LV_USE_XML) && LV_USE_XML
#endif /* LV_USE_XML */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void check_font(lv_font_t ** font, const char * name);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t ui_target = UI_TARGET_ALL;

/*----------------
 * Translations
 *----------------*/

#ifndef LV_EDITOR_PREVIEW
    static const char * translation_languages[] = {"en", "de", "es", NULL};
    static const char * translation_tags[] = {"dark_theme", "buttons", "controls", "text_inputs", "selection", NULL};
    static const char * translation_texts[] = {
        "Dark theme", "Dunkles Design", "Tema oscuro", /* dark_theme */
        "Buttons", "Tasten", "Botones", /* buttons */
        "Controls", "Steuerelemente", "Controles", /* controls */
        "Text inputs", "Texteingaben", "Campos de texto", /* text_inputs */
        "Selection", "Auswahl", "Seleccion", /* selection */
    };
#endif

/**********************
 *  GLOBAL VARIABLES
 **********************/

/*--------------------
 *  Permanent screens
 *-------------------*/

lv_obj_t * age = NULL;
lv_obj_t * berhasil = NULL;
lv_obj_t * gender = NULL;
lv_obj_t * home = NULL;
lv_obj_t * mengukur = NULL;
lv_obj_t * monitor = NULL;
lv_obj_t * result = NULL;
lv_obj_t * scanning = NULL;

/*----------------
 * Fonts
 *----------------*/

lv_font_t * font_inter_regular_36;
lv_font_t * font_inter_semi_bold_20;
lv_font_t * font_inter_semi_bold_16;
lv_font_t * font_inter_bold_32;
lv_font_t * font_inter_medium_16;
lv_font_t * font_inter_semi_bold_36;
lv_font_t * font_inter_bold_48;
lv_font_t * font_inter_semi_bold_18;
lv_font_t * font_inter_regular_14;
lv_font_t * font_inter_regular_13;
lv_font_t * font_inter_semi_bold_13;
lv_font_t * font_inter_regular_10;
lv_font_t * font_inter_regular_16;
lv_font_t * font_poppins_semibold_35_65217590332031;
lv_font_t * font_inter_semi_bold_24;
lv_font_t * font_inter_regular_12;
lv_font_t * font_inter_bold_24;
lv_font_t * font_inter_bold_20;
lv_font_t * font_inter_bold_18;
lv_font_t * font_inter_regular_9;
lv_font_t * font_inter_semi_bold_14;
lv_font_t * font_inter_bold_56;
lv_font_t * font_inter_regular_24;
lv_font_t * font_inter_semi_bold_12;
lv_font_t * font_inter_bold_36;
lv_font_t * font_inter_regular_11;
lv_font_t * font_inter_bold_28;
lv_font_t * font_inter_bold_47;
lv_font_t * font_inter_regular_47;
lv_font_t * font_helvetica_regular_7_000000476837158;
lv_font_t * font_inter_regular_12_000000953674316;
lv_font_t * font_inter_bold_30;
lv_font_t * font_inter_medium_14_000000953674316;
lv_font_t * font_inter_bold_14;
lv_font_t * font_inter_regular_15;
lv_font_t * font_inter_medium_13;
lv_font_t * font_inter_medium_12;
lv_font_t * font_inter_bold_12;
lv_font_t * font_inter_medium_15;
lv_font_t * font_inter_bold_14_000000953674316;
lv_font_t * font_inter_extra_bold_18;
lv_font_t * font_inter_medium_18;
lv_font_t * font_inter_medium_14;
lv_font_t * font_inter_regular_18;
lv_font_t * font_inter_bold_25;
lv_font_t * font_inter_medium_13_999999046325684;
lv_font_t * font_inter_extra_bold_20;
lv_font_t * font_inter_bold_35;
lv_font_t * font_inter_extra_bold_25;
lv_font_t * font_inter_medium_10;
lv_font_t * font_body_symbols;
extern lv_font_t font_body_symbols_data;
lv_font_t * font_body;
extern lv_font_t font_body_data;
lv_font_t * font_h5;
extern lv_font_t font_h5_data;
lv_font_t * font_h4;
extern lv_font_t font_h4_data;
lv_font_t * font_h3;
extern lv_font_t font_h3_data;
lv_font_t * font_h2;
extern lv_font_t font_h2_data;
lv_font_t * font_h1;
extern lv_font_t font_h1_data;

/*----------------
 * Images
 *----------------*/

/* Targets: any */
const void * icon_161221 = NULL;
extern const void * icon_161221_data;
const void * icon_361488 = NULL;
extern const void * icon_361488_data;
const void * icon_63636 = NULL;
extern const void * icon_63636_data;
const void * icon_63640 = NULL;
extern const void * icon_63640_data;
const void * icon_63653 = NULL;
extern const void * icon_63653_data;
const void * icon_63657 = NULL;
extern const void * icon_63657_data;
const void * vec_36_1610 = NULL;
extern const void * vec_36_1610_data;
const void * vec_36_1611 = NULL;
extern const void * vec_36_1611_data;
const void * vec_36_1612 = NULL;
extern const void * vec_36_1612_data;
const void * vec_36_1613 = NULL;
extern const void * vec_36_1613_data;
const void * vec_36_1614 = NULL;
extern const void * vec_36_1614_data;
const void * vec_36_1618 = NULL;
extern const void * vec_36_1618_data;
const void * vec_36_1619 = NULL;
extern const void * vec_36_1619_data;
const void * vec_36_1620 = NULL;
extern const void * vec_36_1620_data;
const void * vec_36_1621 = NULL;
extern const void * vec_36_1621_data;
const void * vec_36_1622 = NULL;
extern const void * vec_36_1622_data;
const void * vec_60_215 = NULL;
extern const void * vec_60_215_data;
const void * vec_60_216 = NULL;
extern const void * vec_60_216_data;
const void * vec_60_217 = NULL;
extern const void * vec_60_217_data;
const void * vec_60_218 = NULL;
extern const void * vec_60_218_data;
const void * vec_60_219 = NULL;
extern const void * vec_60_219_data;
const void * vec_60_224 = NULL;
extern const void * vec_60_224_data;
const void * vec_60_225 = NULL;
extern const void * vec_60_225_data;
const void * vec_60_230 = NULL;
extern const void * vec_60_230_data;
const void * vec_60_231 = NULL;
extern const void * vec_60_231_data;
const void * vec_60_232 = NULL;
extern const void * vec_60_232_data;
const void * vec_282_31 = NULL;
extern const void * vec_282_31_data;
const void * vec_282_32 = NULL;
extern const void * vec_282_32_data;
const void * vec_282_33 = NULL;
extern const void * vec_282_33_data;
const void * vec_282_35 = NULL;
extern const void * vec_282_35_data;
const void * vec_16_385 = NULL;
extern const void * vec_16_385_data;
const void * vec_16_386 = NULL;
extern const void * vec_16_386_data;
const void * vec_16_387 = NULL;
extern const void * vec_16_387_data;
const void * vec_16_388 = NULL;
extern const void * vec_16_388_data;
const void * vec_16_389 = NULL;
extern const void * vec_16_389_data;
const void * vec_36_1588 = NULL;
extern const void * vec_36_1588_data;
const void * vec_36_1589 = NULL;
extern const void * vec_36_1589_data;
const void * vec_36_1590 = NULL;
extern const void * vec_36_1590_data;
const void * vec_36_1591 = NULL;
extern const void * vec_36_1591_data;
const void * vec_36_1592 = NULL;
extern const void * vec_36_1592_data;
const void * vec_36_1596 = NULL;
extern const void * vec_36_1596_data;
const void * vec_36_1597 = NULL;
extern const void * vec_36_1597_data;
const void * vec_36_1598 = NULL;
extern const void * vec_36_1598_data;
const void * vec_36_1599 = NULL;
extern const void * vec_36_1599_data;
const void * vec_36_1600 = NULL;
extern const void * vec_36_1600_data;
const void * vec_60_271 = NULL;
extern const void * vec_60_271_data;
const void * vec_60_272 = NULL;
extern const void * vec_60_272_data;
const void * vec_60_278 = NULL;
extern const void * vec_60_278_data;
const void * vec_60_279 = NULL;
extern const void * vec_60_279_data;
const void * vec_60_284 = NULL;
extern const void * vec_60_284_data;
const void * vec_60_285 = NULL;
extern const void * vec_60_285_data;
const void * vec_60_286 = NULL;
extern const void * vec_60_286_data;
const void * vec_16_583 = NULL;
extern const void * vec_16_583_data;
const void * vec_16_584 = NULL;
extern const void * vec_16_584_data;
const void * vec_16_585 = NULL;
extern const void * vec_16_585_data;
const void * vec_16_586 = NULL;
extern const void * vec_16_586_data;
const void * vec_16_587 = NULL;
extern const void * vec_16_587_data;
const void * vec_36_1566 = NULL;
extern const void * vec_36_1566_data;
const void * vec_36_1567 = NULL;
extern const void * vec_36_1567_data;
const void * vec_36_1568 = NULL;
extern const void * vec_36_1568_data;
const void * vec_36_1569 = NULL;
extern const void * vec_36_1569_data;
const void * vec_36_1570 = NULL;
extern const void * vec_36_1570_data;
const void * vec_36_1574 = NULL;
extern const void * vec_36_1574_data;
const void * vec_36_1575 = NULL;
extern const void * vec_36_1575_data;
const void * vec_36_1576 = NULL;
extern const void * vec_36_1576_data;
const void * vec_36_1577 = NULL;
extern const void * vec_36_1577_data;
const void * vec_36_1578 = NULL;
extern const void * vec_36_1578_data;
const void * vec_60_145 = NULL;
extern const void * vec_60_145_data;
const void * vec_60_150 = NULL;
extern const void * vec_60_150_data;
const void * vec_60_151 = NULL;
extern const void * vec_60_151_data;
const void * vec_60_156 = NULL;
extern const void * vec_60_156_data;
const void * vec_60_157 = NULL;
extern const void * vec_60_157_data;
const void * vec_60_162 = NULL;
extern const void * vec_60_162_data;
const void * vec_60_163 = NULL;
extern const void * vec_60_163_data;
const void * vec_60_164 = NULL;
extern const void * vec_60_164_data;
const void * vec_16_1205 = NULL;
extern const void * vec_16_1205_data;
const void * vec_16_1206 = NULL;
extern const void * vec_16_1206_data;
const void * vec_16_1207 = NULL;
extern const void * vec_16_1207_data;
const void * vec_68_1023 = NULL;
extern const void * vec_68_1023_data;
const void * vec_68_1024 = NULL;
extern const void * vec_68_1024_data;
const void * vec_16_1235 = NULL;
extern const void * vec_16_1235_data;
const void * vec_16_1236 = NULL;
extern const void * vec_16_1236_data;
const void * vec_16_1237 = NULL;
extern const void * vec_16_1237_data;
const void * vec_68_865 = NULL;
extern const void * vec_68_865_data;
const void * vec_68_866 = NULL;
extern const void * vec_68_866_data;
const void * vec_36_1544 = NULL;
extern const void * vec_36_1544_data;
const void * vec_36_1545 = NULL;
extern const void * vec_36_1545_data;
const void * vec_36_1546 = NULL;
extern const void * vec_36_1546_data;
const void * vec_36_1547 = NULL;
extern const void * vec_36_1547_data;
const void * vec_36_1548 = NULL;
extern const void * vec_36_1548_data;
const void * vec_36_1552 = NULL;
extern const void * vec_36_1552_data;
const void * vec_36_1553 = NULL;
extern const void * vec_36_1553_data;
const void * vec_36_1554 = NULL;
extern const void * vec_36_1554_data;
const void * vec_36_1555 = NULL;
extern const void * vec_36_1555_data;
const void * vec_36_1556 = NULL;
extern const void * vec_36_1556_data;
const void * vec_63_375 = NULL;
extern const void * vec_63_375_data;
const void * vec_63_376 = NULL;
extern const void * vec_63_376_data;
const void * vec_60_124 = NULL;
extern const void * vec_60_124_data;
const void * vec_60_125 = NULL;
extern const void * vec_60_125_data;
const void * vec_60_130 = NULL;
extern const void * vec_60_130_data;
const void * vec_60_131 = NULL;
extern const void * vec_60_131_data;
const void * vec_60_136 = NULL;
extern const void * vec_60_136_data;
const void * vec_60_137 = NULL;
extern const void * vec_60_137_data;
const void * vec_60_138 = NULL;
extern const void * vec_60_138_data;
const void * vec_36_1497 = NULL;
extern const void * vec_36_1497_data;
const void * vec_36_1498 = NULL;
extern const void * vec_36_1498_data;
const void * vec_36_1499 = NULL;
extern const void * vec_36_1499_data;
const void * vec_68_1019 = NULL;
extern const void * vec_68_1019_data;
const void * vec_68_1020 = NULL;
extern const void * vec_68_1020_data;
const void * vec_68_1007 = NULL;
extern const void * vec_68_1007_data;
const void * vec_68_1008 = NULL;
extern const void * vec_68_1008_data;
const void * vec_36_1522 = NULL;
extern const void * vec_36_1522_data;
const void * vec_36_1523 = NULL;
extern const void * vec_36_1523_data;
const void * vec_36_1524 = NULL;
extern const void * vec_36_1524_data;
const void * vec_36_1525 = NULL;
extern const void * vec_36_1525_data;
const void * vec_36_1526 = NULL;
extern const void * vec_36_1526_data;
const void * vec_36_1530 = NULL;
extern const void * vec_36_1530_data;
const void * vec_36_1531 = NULL;
extern const void * vec_36_1531_data;
const void * vec_36_1532 = NULL;
extern const void * vec_36_1532_data;
const void * vec_36_1533 = NULL;
extern const void * vec_36_1533_data;
const void * vec_36_1534 = NULL;
extern const void * vec_36_1534_data;
const void * vec_63_369 = NULL;
extern const void * vec_63_369_data;
const void * vec_63_370 = NULL;
extern const void * vec_63_370_data;
const void * vec_60_104 = NULL;
extern const void * vec_60_104_data;
const void * vec_60_105 = NULL;
extern const void * vec_60_105_data;
const void * vec_60_110 = NULL;
extern const void * vec_60_110_data;
const void * vec_60_111 = NULL;
extern const void * vec_60_111_data;
const void * vec_60_112 = NULL;
extern const void * vec_60_112_data;
const void * vec_60_3 = NULL;
extern const void * vec_60_3_data;
const void * vec_60_12 = NULL;
extern const void * vec_60_12_data;
const void * vec_60_82 = NULL;
extern const void * vec_60_82_data;
const void * vec_60_83 = NULL;
extern const void * vec_60_83_data;
const void * vec_60_88 = NULL;
extern const void * vec_60_88_data;
const void * vec_56_1838 = NULL;
extern const void * vec_56_1838_data;
const void * vec_56_1839 = NULL;
extern const void * vec_56_1839_data;
const void * vec_56_1840 = NULL;
extern const void * vec_56_1840_data;
const void * vec_56_1841 = NULL;
extern const void * vec_56_1841_data;
const void * vec_56_1842 = NULL;
extern const void * vec_56_1842_data;
const void * vec_56_1846 = NULL;
extern const void * vec_56_1846_data;
const void * vec_56_1847 = NULL;
extern const void * vec_56_1847_data;
const void * vec_56_1848 = NULL;
extern const void * vec_56_1848_data;
const void * vec_56_1849 = NULL;
extern const void * vec_56_1849_data;
const void * vec_56_1850 = NULL;
extern const void * vec_56_1850_data;
const void * vec_60_294 = NULL;
extern const void * vec_60_294_data;
const void * vec_60_299 = NULL;
extern const void * vec_60_299_data;
const void * vec_60_304 = NULL;
extern const void * vec_60_304_data;
const void * vec_60_305 = NULL;
extern const void * vec_60_305_data;
const void * vec_60_310 = NULL;
extern const void * vec_60_310_data;
const void * vec_60_317 = NULL;
extern const void * vec_60_317_data;
const void * vec_60_318 = NULL;
extern const void * vec_60_318_data;
const void * vec_60_319 = NULL;
extern const void * vec_60_319_data;
const void * vec_60_320 = NULL;
extern const void * vec_60_320_data;
const void * vec_60_321 = NULL;
extern const void * vec_60_321_data;
const void * vec_60_325 = NULL;
extern const void * vec_60_325_data;
const void * vec_60_326 = NULL;
extern const void * vec_60_326_data;
const void * vec_60_327 = NULL;
extern const void * vec_60_327_data;
const void * vec_60_328 = NULL;
extern const void * vec_60_328_data;
const void * vec_60_329 = NULL;
extern const void * vec_60_329_data;
const void * vec_60_348 = NULL;
extern const void * vec_60_348_data;
const void * vec_60_349 = NULL;
extern const void * vec_60_349_data;
const void * vec_60_350 = NULL;
extern const void * vec_60_350_data;
const void * vec_63_363 = NULL;
extern const void * vec_63_363_data;
const void * vec_63_364 = NULL;
extern const void * vec_63_364_data;
const void * vec_63_365 = NULL;
extern const void * vec_63_365_data;
const void * vec_63_418 = NULL;
extern const void * vec_63_418_data;
const void * vec_63_419 = NULL;
extern const void * vec_63_419_data;
const void * vec_63_420 = NULL;
extern const void * vec_63_420_data;
const void * vec_63_421 = NULL;
extern const void * vec_63_421_data;
const void * vec_63_422 = NULL;
extern const void * vec_63_422_data;
const void * vec_63_426 = NULL;
extern const void * vec_63_426_data;
const void * vec_63_427 = NULL;
extern const void * vec_63_427_data;
const void * vec_63_428 = NULL;
extern const void * vec_63_428_data;
const void * vec_63_429 = NULL;
extern const void * vec_63_429_data;
const void * vec_63_430 = NULL;
extern const void * vec_63_430_data;
const void * vec_63_834 = NULL;
extern const void * vec_63_834_data;
const void * vec_63_835 = NULL;
extern const void * vec_63_835_data;
const void * vec_63_840 = NULL;
extern const void * vec_63_840_data;
const void * vec_63_841 = NULL;
extern const void * vec_63_841_data;
const void * vec_63_450 = NULL;
extern const void * vec_63_450_data;
const void * vec_63_451 = NULL;
extern const void * vec_63_451_data;
const void * vec_63_456 = NULL;
extern const void * vec_63_456_data;
const void * vec_63_457 = NULL;
extern const void * vec_63_457_data;
const void * vec_63_458 = NULL;
extern const void * vec_63_458_data;
const void * vec_63_667 = NULL;
extern const void * vec_63_667_data;
const void * vec_63_668 = NULL;
extern const void * vec_63_668_data;
const void * vec_63_669 = NULL;
extern const void * vec_63_669_data;
const void * vec_63_673 = NULL;
extern const void * vec_63_673_data;
const void * vec_63_674 = NULL;
extern const void * vec_63_674_data;
const void * vec_63_687 = NULL;
extern const void * vec_63_687_data;
const void * vec_63_688 = NULL;
extern const void * vec_63_688_data;
const void * vec_63_689 = NULL;
extern const void * vec_63_689_data;
const void * icon_arrow_left = NULL;
extern const void * icon_arrow_left_data;
const void * icon_check = NULL;
extern const void * icon_check_data;
const void * icon_chevron_down = NULL;
extern const void * icon_chevron_down_data;
const void * icon_chevron_up = NULL;
extern const void * icon_chevron_up_data;
const void * icon_close = NULL;
extern const void * icon_close_data;
const void * icon_gender_male = NULL;
extern const void * icon_gender_male_data;
const void * icon_gender_female = NULL;
extern const void * icon_gender_female_data;
const void * icon_monitor = NULL;
extern const void * icon_monitor_data;
const void * icon_priority_immediate = NULL;
extern const void * icon_priority_immediate_data;
const void * icon_priority_delayed = NULL;
extern const void * icon_priority_delayed_data;
const void * icon_priority_minor = NULL;
extern const void * icon_priority_minor_data;
const void * icon_vital_spo2_sm = NULL;
extern const void * icon_vital_spo2_sm_data;
const void * icon_vital_hr_sm = NULL;
extern const void * icon_vital_hr_sm_data;
const void * icon_vital_rr_sm = NULL;
extern const void * icon_vital_rr_sm_data;
const void * icon_vital_bp_sm = NULL;
extern const void * icon_vital_bp_sm_data;
const void * icon_update = NULL;
extern const void * icon_update_data;
const void * icon_heart_pulse = NULL;
extern const void * icon_heart_pulse_data;
const void * icon_oxygen = NULL;
extern const void * icon_oxygen_data;
const void * icon_respiratory = NULL;
extern const void * icon_respiratory_data;
const void * icon_blood_pressure = NULL;
extern const void * icon_blood_pressure_data;
const void * icon_menu = NULL;
extern const void * icon_menu_data;
const void * icon_pause = NULL;
extern const void * icon_pause_data;
const void * icon_play = NULL;
extern const void * icon_play_data;
const void * icon_power = NULL;
extern const void * icon_power_data;
const void * icon_refresh = NULL;
extern const void * icon_refresh_data;
const void * icon_search = NULL;
extern const void * icon_search_data;
const void * icon_rfid_scan = NULL;
extern const void * icon_rfid_scan_data;
const void * icon_rfid_scan_lg = NULL;
extern const void * icon_rfid_scan_lg_data;
const void * icon_signal = NULL;
extern const void * icon_signal_data;
const void * icon_star = NULL;
extern const void * icon_star_data;
const void * icon_wifi_high = NULL;
extern const void * icon_wifi_high_data;
const void * icon_wifi_low = NULL;
extern const void * icon_wifi_low_data;
const void * icon_wifi_zero = NULL;
extern const void * icon_wifi_zero_data;
const void * logo_light_for_dark = NULL;
extern const void * logo_light_for_dark_data;
const void * battery_empty = NULL;
extern const void * battery_empty_data;
const void * battery_medium = NULL;
extern const void * battery_medium_data;
const void * battery_full = NULL;
extern const void * battery_full_data;
const void * battery_charging = NULL;
extern const void * battery_charging_data;

/*----------------
 * Global styles
 *----------------*/

lv_style_t shadow_button;
lv_style_t shadow_icon;
lv_style_t screen_base;
lv_style_t style_screen_light;
lv_style_t style_screen_dark;
lv_style_t style_panel_light;
lv_style_t style_panel_dark;
lv_style_t style_text_accent;
lv_style_t style_text_muted;
lv_style_t style_scrollbar;
lv_style_t style_buttonbar_cell_grad;

/*----------------
 * Subjects
 *----------------*/

lv_subject_t subject_theme_dark;
lv_subject_t subject_brightness;
lv_subject_t subject_show_keyboard;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ui_init_gen(const char * asset_path)
{
    char buf[256];

    /* When running from the editor the theme set from the XML should overwrite this */
#if !defined(LV_EDITOR_PREVIEW)
#if LV_USE_THEME_SIMPLE
    lv_display_t * disp = lv_display_get_default();
    lv_theme_t * th = lv_theme_simple_init(disp);
    lv_display_set_theme(disp, th);
#else
    LV_LOG_WARN("Simple theme is selected in project.xml but LV_USE_THEME_SIMPLE is disabled");
#endif
#endif /*LV_EDITOR_PREVIEW*/


    /*----------------
     * Fonts
     *----------------*/

    /* Targets: any */

    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        if (!font_inter_regular_36) {
            /* font_inter_regular_36 */
            /* create tiny ttf font "font_inter_regular_36" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_36 = lv_tiny_ttf_create_file(buf, 36);
        }
        if (!font_inter_semi_bold_20) {
            /* font_inter_semi_bold_20 */
            /* create tiny ttf font "font_inter_semi_bold_20" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_20 = lv_tiny_ttf_create_file(buf, 20);
        }
        if (!font_inter_semi_bold_16) {
            /* font_inter_semi_bold_16 */
            /* create tiny ttf font "font_inter_semi_bold_16" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_16 = lv_tiny_ttf_create_file(buf, 16);
        }
        if (!font_inter_bold_32) {
            /* font_inter_bold_32 */
            /* create tiny ttf font "font_inter_bold_32" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_32 = lv_tiny_ttf_create_file(buf, 32);
        }
        if (!font_inter_medium_16) {
            /* font_inter_medium_16 */
            /* create tiny ttf font "font_inter_medium_16" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_16 = lv_tiny_ttf_create_file(buf, 16);
        }
        if (!font_inter_semi_bold_36) {
            /* font_inter_semi_bold_36 */
            /* create tiny ttf font "font_inter_semi_bold_36" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_36 = lv_tiny_ttf_create_file(buf, 36);
        }
        if (!font_inter_bold_48) {
            /* font_inter_bold_48 */
            /* create tiny ttf font "font_inter_bold_48" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_48 = lv_tiny_ttf_create_file(buf, 48);
        }
        if (!font_inter_semi_bold_18) {
            /* font_inter_semi_bold_18 */
            /* create tiny ttf font "font_inter_semi_bold_18" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_18 = lv_tiny_ttf_create_file(buf, 18);
        }
        if (!font_inter_regular_14) {
            /* font_inter_regular_14 */
            /* create tiny ttf font "font_inter_regular_14" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_14 = lv_tiny_ttf_create_file(buf, 14);
        }
        if (!font_inter_regular_13) {
            /* font_inter_regular_13 */
            /* create tiny ttf font "font_inter_regular_13" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_13 = lv_tiny_ttf_create_file(buf, 13);
        }
        if (!font_inter_semi_bold_13) {
            /* font_inter_semi_bold_13 */
            /* create tiny ttf font "font_inter_semi_bold_13" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_13 = lv_tiny_ttf_create_file(buf, 13);
        }
        if (!font_inter_regular_10) {
            /* font_inter_regular_10 */
            /* create tiny ttf font "font_inter_regular_10" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_10 = lv_tiny_ttf_create_file(buf, 10);
        }
        if (!font_inter_regular_16) {
            /* font_inter_regular_16 */
            /* create tiny ttf font "font_inter_regular_16" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_16 = lv_tiny_ttf_create_file(buf, 16);
        }
        if (!font_poppins_semibold_35_65217590332031) {
            /* font_poppins_semibold_35_65217590332031 */
            /* create tiny ttf font "font_poppins_semibold_35_65217590332031" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Poppins-SemiBold.ttf");
            font_poppins_semibold_35_65217590332031 = lv_tiny_ttf_create_file(buf, 36);
        }
        if (!font_inter_semi_bold_24) {
            /* font_inter_semi_bold_24 */
            /* create tiny ttf font "font_inter_semi_bold_24" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_24 = lv_tiny_ttf_create_file(buf, 24);
        }
        if (!font_inter_regular_12) {
            /* font_inter_regular_12 */
            /* create tiny ttf font "font_inter_regular_12" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_12 = lv_tiny_ttf_create_file(buf, 12);
        }
        if (!font_inter_bold_24) {
            /* font_inter_bold_24 */
            /* create tiny ttf font "font_inter_bold_24" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_24 = lv_tiny_ttf_create_file(buf, 24);
        }
        if (!font_inter_bold_20) {
            /* font_inter_bold_20 */
            /* create tiny ttf font "font_inter_bold_20" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_20 = lv_tiny_ttf_create_file(buf, 20);
        }
        if (!font_inter_bold_18) {
            /* font_inter_bold_18 */
            /* create tiny ttf font "font_inter_bold_18" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_18 = lv_tiny_ttf_create_file(buf, 18);
        }
        if (!font_inter_regular_9) {
            /* font_inter_regular_9 */
            /* create tiny ttf font "font_inter_regular_9" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_9 = lv_tiny_ttf_create_file(buf, 9);
        }
        if (!font_inter_semi_bold_14) {
            /* font_inter_semi_bold_14 */
            /* create tiny ttf font "font_inter_semi_bold_14" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_14 = lv_tiny_ttf_create_file(buf, 14);
        }
        if (!font_inter_bold_56) {
            /* font_inter_bold_56 */
            /* create tiny ttf font "font_inter_bold_56" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_56 = lv_tiny_ttf_create_file(buf, 56);
        }
        if (!font_inter_regular_24) {
            /* font_inter_regular_24 */
            /* create tiny ttf font "font_inter_regular_24" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_24 = lv_tiny_ttf_create_file(buf, 24);
        }
        if (!font_inter_semi_bold_12) {
            /* font_inter_semi_bold_12 */
            /* create tiny ttf font "font_inter_semi_bold_12" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-SemiBold.ttf");
            font_inter_semi_bold_12 = lv_tiny_ttf_create_file(buf, 12);
        }
        if (!font_inter_bold_36) {
            /* font_inter_bold_36 */
            /* create tiny ttf font "font_inter_bold_36" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_36 = lv_tiny_ttf_create_file(buf, 36);
        }
        if (!font_inter_regular_11) {
            /* font_inter_regular_11 */
            /* create tiny ttf font "font_inter_regular_11" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_11 = lv_tiny_ttf_create_file(buf, 11);
        }
        if (!font_inter_bold_28) {
            /* font_inter_bold_28 */
            /* create tiny ttf font "font_inter_bold_28" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_28 = lv_tiny_ttf_create_file(buf, 28);
        }
        if (!font_inter_bold_47) {
            /* font_inter_bold_47 */
            /* create tiny ttf font "font_inter_bold_47" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_47 = lv_tiny_ttf_create_file(buf, 47);
        }
        if (!font_inter_regular_47) {
            /* font_inter_regular_47 */
            /* create tiny ttf font "font_inter_regular_47" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_47 = lv_tiny_ttf_create_file(buf, 47);
        }
        if (!font_helvetica_regular_7_000000476837158) {
            /* font_helvetica_regular_7_000000476837158 */
            /* create tiny ttf font "font_helvetica_regular_7_000000476837158" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Helvetica-Regular.ttf");
            font_helvetica_regular_7_000000476837158 = lv_tiny_ttf_create_file(buf, 7);
        }
        if (!font_inter_regular_12_000000953674316) {
            /* font_inter_regular_12_000000953674316 */
            /* create tiny ttf font "font_inter_regular_12_000000953674316" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_12_000000953674316 = lv_tiny_ttf_create_file(buf, 12);
        }
        if (!font_inter_bold_30) {
            /* font_inter_bold_30 */
            /* create tiny ttf font "font_inter_bold_30" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_30 = lv_tiny_ttf_create_file(buf, 30);
        }
        if (!font_inter_medium_14_000000953674316) {
            /* font_inter_medium_14_000000953674316 */
            /* create tiny ttf font "font_inter_medium_14_000000953674316" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_14_000000953674316 = lv_tiny_ttf_create_file(buf, 14);
        }
        if (!font_inter_bold_14) {
            /* font_inter_bold_14 */
            /* create tiny ttf font "font_inter_bold_14" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_14 = lv_tiny_ttf_create_file(buf, 14);
        }
        if (!font_inter_regular_15) {
            /* font_inter_regular_15 */
            /* create tiny ttf font "font_inter_regular_15" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_15 = lv_tiny_ttf_create_file(buf, 15);
        }
        if (!font_inter_medium_13) {
            /* font_inter_medium_13 */
            /* create tiny ttf font "font_inter_medium_13" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_13 = lv_tiny_ttf_create_file(buf, 13);
        }
        if (!font_inter_medium_12) {
            /* font_inter_medium_12 */
            /* create tiny ttf font "font_inter_medium_12" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_12 = lv_tiny_ttf_create_file(buf, 12);
        }
        if (!font_inter_bold_12) {
            /* font_inter_bold_12 */
            /* create tiny ttf font "font_inter_bold_12" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_12 = lv_tiny_ttf_create_file(buf, 12);
        }
        if (!font_inter_medium_15) {
            /* font_inter_medium_15 */
            /* create tiny ttf font "font_inter_medium_15" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_15 = lv_tiny_ttf_create_file(buf, 15);
        }
        if (!font_inter_bold_14_000000953674316) {
            /* font_inter_bold_14_000000953674316 */
            /* create tiny ttf font "font_inter_bold_14_000000953674316" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_14_000000953674316 = lv_tiny_ttf_create_file(buf, 14);
        }
        if (!font_inter_extra_bold_18) {
            /* font_inter_extra_bold_18 */
            /* create tiny ttf font "font_inter_extra_bold_18" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-ExtraBold.ttf");
            font_inter_extra_bold_18 = lv_tiny_ttf_create_file(buf, 18);
        }
        if (!font_inter_medium_18) {
            /* font_inter_medium_18 */
            /* create tiny ttf font "font_inter_medium_18" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_18 = lv_tiny_ttf_create_file(buf, 18);
        }
        if (!font_inter_medium_14) {
            /* font_inter_medium_14 */
            /* create tiny ttf font "font_inter_medium_14" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_14 = lv_tiny_ttf_create_file(buf, 14);
        }
        if (!font_inter_regular_18) {
            /* font_inter_regular_18 */
            /* create tiny ttf font "font_inter_regular_18" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Regular.ttf");
            font_inter_regular_18 = lv_tiny_ttf_create_file(buf, 18);
        }
        if (!font_inter_bold_25) {
            /* font_inter_bold_25 */
            /* create tiny ttf font "font_inter_bold_25" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_25 = lv_tiny_ttf_create_file(buf, 25);
        }
        if (!font_inter_medium_13_999999046325684) {
            /* font_inter_medium_13_999999046325684 */
            /* create tiny ttf font "font_inter_medium_13_999999046325684" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_13_999999046325684 = lv_tiny_ttf_create_file(buf, 14);
        }
        if (!font_inter_extra_bold_20) {
            /* font_inter_extra_bold_20 */
            /* create tiny ttf font "font_inter_extra_bold_20" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-ExtraBold.ttf");
            font_inter_extra_bold_20 = lv_tiny_ttf_create_file(buf, 20);
        }
        if (!font_inter_bold_35) {
            /* font_inter_bold_35 */
            /* create tiny ttf font "font_inter_bold_35" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Bold.ttf");
            font_inter_bold_35 = lv_tiny_ttf_create_file(buf, 35);
        }
        if (!font_inter_extra_bold_25) {
            /* font_inter_extra_bold_25 */
            /* create tiny ttf font "font_inter_extra_bold_25" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-ExtraBold.ttf");
            font_inter_extra_bold_25 = lv_tiny_ttf_create_file(buf, 25);
        }
        if (!font_inter_medium_10) {
            /* font_inter_medium_10 */
            /* create tiny ttf font "font_inter_medium_10" from file */
            lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Inter-Medium.ttf");
            font_inter_medium_10 = lv_tiny_ttf_create_file(buf, 10);
        }
        if (!font_body_symbols) {
            /* font_body_symbols */
            /* get font 'font_body_symbols' from a C array */
            font_body_symbols = &font_body_symbols_data;

        }
        if (!font_body) {
            /* font_body */
            /* get font 'font_body' from a C array */
            font_body = &font_body_data;

        }
        if (!font_h5) {
            /* font_h5 */
            /* get font 'font_h5' from a C array */
            font_h5 = &font_h5_data;

        }
        if (!font_h4) {
            /* font_h4 */
            /* get font 'font_h4' from a C array */
            font_h4 = &font_h4_data;

        }
        if (!font_h3) {
            /* font_h3 */
            /* get font 'font_h3' from a C array */
            font_h3 = &font_h3_data;

        }
        if (!font_h2) {
            /* font_h2 */
            /* get font 'font_h2' from a C array */
            font_h2 = &font_h2_data;

        }
        if (!font_h1) {
            /* font_h1 */
            /* get font 'font_h1' from a C array */
            font_h1 = &font_h1_data;

        }
    }
    #endif

    /*----------------
     * Images
     *----------------*/

    /* Targets: any */
    #if UI_CHECK_COMPILE_TARGET(UI_TARGET_ALL)
    if (ui_check_target(UI_TARGET_ALL)) {
        /* icon_161221 */
        if (!icon_161221) {
            icon_161221 = &icon_161221_data;
        }
        /* icon_361488 */
        if (!icon_361488) {
            icon_361488 = &icon_361488_data;
        }
        /* icon_63636 */
        if (!icon_63636) {
            icon_63636 = &icon_63636_data;
        }
        /* icon_63640 */
        if (!icon_63640) {
            icon_63640 = &icon_63640_data;
        }
        /* icon_63653 */
        if (!icon_63653) {
            icon_63653 = &icon_63653_data;
        }
        /* icon_63657 */
        if (!icon_63657) {
            icon_63657 = &icon_63657_data;
        }
        /* vec_36_1610 */
        if (!vec_36_1610) {
            vec_36_1610 = &vec_36_1610_data;
        }
        /* vec_36_1611 */
        if (!vec_36_1611) {
            vec_36_1611 = &vec_36_1611_data;
        }
        /* vec_36_1612 */
        if (!vec_36_1612) {
            vec_36_1612 = &vec_36_1612_data;
        }
        /* vec_36_1613 */
        if (!vec_36_1613) {
            vec_36_1613 = &vec_36_1613_data;
        }
        /* vec_36_1614 */
        if (!vec_36_1614) {
            vec_36_1614 = &vec_36_1614_data;
        }
        /* vec_36_1618 */
        if (!vec_36_1618) {
            vec_36_1618 = &vec_36_1618_data;
        }
        /* vec_36_1619 */
        if (!vec_36_1619) {
            vec_36_1619 = &vec_36_1619_data;
        }
        /* vec_36_1620 */
        if (!vec_36_1620) {
            vec_36_1620 = &vec_36_1620_data;
        }
        /* vec_36_1621 */
        if (!vec_36_1621) {
            vec_36_1621 = &vec_36_1621_data;
        }
        /* vec_36_1622 */
        if (!vec_36_1622) {
            vec_36_1622 = &vec_36_1622_data;
        }
        /* vec_60_215 */
        if (!vec_60_215) {
            vec_60_215 = &vec_60_215_data;
        }
        /* vec_60_216 */
        if (!vec_60_216) {
            vec_60_216 = &vec_60_216_data;
        }
        /* vec_60_217 */
        if (!vec_60_217) {
            vec_60_217 = &vec_60_217_data;
        }
        /* vec_60_218 */
        if (!vec_60_218) {
            vec_60_218 = &vec_60_218_data;
        }
        /* vec_60_219 */
        if (!vec_60_219) {
            vec_60_219 = &vec_60_219_data;
        }
        /* vec_60_224 */
        if (!vec_60_224) {
            vec_60_224 = &vec_60_224_data;
        }
        /* vec_60_225 */
        if (!vec_60_225) {
            vec_60_225 = &vec_60_225_data;
        }
        /* vec_60_230 */
        if (!vec_60_230) {
            vec_60_230 = &vec_60_230_data;
        }
        /* vec_60_231 */
        if (!vec_60_231) {
            vec_60_231 = &vec_60_231_data;
        }
        /* vec_60_232 */
        if (!vec_60_232) {
            vec_60_232 = &vec_60_232_data;
        }
        /* vec_282_31 */
        if (!vec_282_31) {
            vec_282_31 = &vec_282_31_data;
        }
        /* vec_282_32 */
        if (!vec_282_32) {
            vec_282_32 = &vec_282_32_data;
        }
        /* vec_282_33 */
        if (!vec_282_33) {
            vec_282_33 = &vec_282_33_data;
        }
        /* vec_282_35 */
        if (!vec_282_35) {
            vec_282_35 = &vec_282_35_data;
        }
        /* vec_16_385 */
        if (!vec_16_385) {
            vec_16_385 = &vec_16_385_data;
        }
        /* vec_16_386 */
        if (!vec_16_386) {
            vec_16_386 = &vec_16_386_data;
        }
        /* vec_16_387 */
        if (!vec_16_387) {
            vec_16_387 = &vec_16_387_data;
        }
        /* vec_16_388 */
        if (!vec_16_388) {
            vec_16_388 = &vec_16_388_data;
        }
        /* vec_16_389 */
        if (!vec_16_389) {
            vec_16_389 = &vec_16_389_data;
        }
        /* vec_36_1588 */
        if (!vec_36_1588) {
            vec_36_1588 = &vec_36_1588_data;
        }
        /* vec_36_1589 */
        if (!vec_36_1589) {
            vec_36_1589 = &vec_36_1589_data;
        }
        /* vec_36_1590 */
        if (!vec_36_1590) {
            vec_36_1590 = &vec_36_1590_data;
        }
        /* vec_36_1591 */
        if (!vec_36_1591) {
            vec_36_1591 = &vec_36_1591_data;
        }
        /* vec_36_1592 */
        if (!vec_36_1592) {
            vec_36_1592 = &vec_36_1592_data;
        }
        /* vec_36_1596 */
        if (!vec_36_1596) {
            vec_36_1596 = &vec_36_1596_data;
        }
        /* vec_36_1597 */
        if (!vec_36_1597) {
            vec_36_1597 = &vec_36_1597_data;
        }
        /* vec_36_1598 */
        if (!vec_36_1598) {
            vec_36_1598 = &vec_36_1598_data;
        }
        /* vec_36_1599 */
        if (!vec_36_1599) {
            vec_36_1599 = &vec_36_1599_data;
        }
        /* vec_36_1600 */
        if (!vec_36_1600) {
            vec_36_1600 = &vec_36_1600_data;
        }
        /* vec_60_271 */
        if (!vec_60_271) {
            vec_60_271 = &vec_60_271_data;
        }
        /* vec_60_272 */
        if (!vec_60_272) {
            vec_60_272 = &vec_60_272_data;
        }
        /* vec_60_278 */
        if (!vec_60_278) {
            vec_60_278 = &vec_60_278_data;
        }
        /* vec_60_279 */
        if (!vec_60_279) {
            vec_60_279 = &vec_60_279_data;
        }
        /* vec_60_284 */
        if (!vec_60_284) {
            vec_60_284 = &vec_60_284_data;
        }
        /* vec_60_285 */
        if (!vec_60_285) {
            vec_60_285 = &vec_60_285_data;
        }
        /* vec_60_286 */
        if (!vec_60_286) {
            vec_60_286 = &vec_60_286_data;
        }
        /* vec_16_583 */
        if (!vec_16_583) {
            vec_16_583 = &vec_16_583_data;
        }
        /* vec_16_584 */
        if (!vec_16_584) {
            vec_16_584 = &vec_16_584_data;
        }
        /* vec_16_585 */
        if (!vec_16_585) {
            vec_16_585 = &vec_16_585_data;
        }
        /* vec_16_586 */
        if (!vec_16_586) {
            vec_16_586 = &vec_16_586_data;
        }
        /* vec_16_587 */
        if (!vec_16_587) {
            vec_16_587 = &vec_16_587_data;
        }
        /* vec_36_1566 */
        if (!vec_36_1566) {
            vec_36_1566 = &vec_36_1566_data;
        }
        /* vec_36_1567 */
        if (!vec_36_1567) {
            vec_36_1567 = &vec_36_1567_data;
        }
        /* vec_36_1568 */
        if (!vec_36_1568) {
            vec_36_1568 = &vec_36_1568_data;
        }
        /* vec_36_1569 */
        if (!vec_36_1569) {
            vec_36_1569 = &vec_36_1569_data;
        }
        /* vec_36_1570 */
        if (!vec_36_1570) {
            vec_36_1570 = &vec_36_1570_data;
        }
        /* vec_36_1574 */
        if (!vec_36_1574) {
            vec_36_1574 = &vec_36_1574_data;
        }
        /* vec_36_1575 */
        if (!vec_36_1575) {
            vec_36_1575 = &vec_36_1575_data;
        }
        /* vec_36_1576 */
        if (!vec_36_1576) {
            vec_36_1576 = &vec_36_1576_data;
        }
        /* vec_36_1577 */
        if (!vec_36_1577) {
            vec_36_1577 = &vec_36_1577_data;
        }
        /* vec_36_1578 */
        if (!vec_36_1578) {
            vec_36_1578 = &vec_36_1578_data;
        }
        /* vec_60_145 */
        if (!vec_60_145) {
            vec_60_145 = &vec_60_145_data;
        }
        /* vec_60_150 */
        if (!vec_60_150) {
            vec_60_150 = &vec_60_150_data;
        }
        /* vec_60_151 */
        if (!vec_60_151) {
            vec_60_151 = &vec_60_151_data;
        }
        /* vec_60_156 */
        if (!vec_60_156) {
            vec_60_156 = &vec_60_156_data;
        }
        /* vec_60_157 */
        if (!vec_60_157) {
            vec_60_157 = &vec_60_157_data;
        }
        /* vec_60_162 */
        if (!vec_60_162) {
            vec_60_162 = &vec_60_162_data;
        }
        /* vec_60_163 */
        if (!vec_60_163) {
            vec_60_163 = &vec_60_163_data;
        }
        /* vec_60_164 */
        if (!vec_60_164) {
            vec_60_164 = &vec_60_164_data;
        }
        /* vec_16_1205 */
        if (!vec_16_1205) {
            vec_16_1205 = &vec_16_1205_data;
        }
        /* vec_16_1206 */
        if (!vec_16_1206) {
            vec_16_1206 = &vec_16_1206_data;
        }
        /* vec_16_1207 */
        if (!vec_16_1207) {
            vec_16_1207 = &vec_16_1207_data;
        }
        /* vec_68_1023 */
        if (!vec_68_1023) {
            vec_68_1023 = &vec_68_1023_data;
        }
        /* vec_68_1024 */
        if (!vec_68_1024) {
            vec_68_1024 = &vec_68_1024_data;
        }
        /* vec_16_1235 */
        if (!vec_16_1235) {
            vec_16_1235 = &vec_16_1235_data;
        }
        /* vec_16_1236 */
        if (!vec_16_1236) {
            vec_16_1236 = &vec_16_1236_data;
        }
        /* vec_16_1237 */
        if (!vec_16_1237) {
            vec_16_1237 = &vec_16_1237_data;
        }
        /* vec_68_865 */
        if (!vec_68_865) {
            vec_68_865 = &vec_68_865_data;
        }
        /* vec_68_866 */
        if (!vec_68_866) {
            vec_68_866 = &vec_68_866_data;
        }
        /* vec_36_1544 */
        if (!vec_36_1544) {
            vec_36_1544 = &vec_36_1544_data;
        }
        /* vec_36_1545 */
        if (!vec_36_1545) {
            vec_36_1545 = &vec_36_1545_data;
        }
        /* vec_36_1546 */
        if (!vec_36_1546) {
            vec_36_1546 = &vec_36_1546_data;
        }
        /* vec_36_1547 */
        if (!vec_36_1547) {
            vec_36_1547 = &vec_36_1547_data;
        }
        /* vec_36_1548 */
        if (!vec_36_1548) {
            vec_36_1548 = &vec_36_1548_data;
        }
        /* vec_36_1552 */
        if (!vec_36_1552) {
            vec_36_1552 = &vec_36_1552_data;
        }
        /* vec_36_1553 */
        if (!vec_36_1553) {
            vec_36_1553 = &vec_36_1553_data;
        }
        /* vec_36_1554 */
        if (!vec_36_1554) {
            vec_36_1554 = &vec_36_1554_data;
        }
        /* vec_36_1555 */
        if (!vec_36_1555) {
            vec_36_1555 = &vec_36_1555_data;
        }
        /* vec_36_1556 */
        if (!vec_36_1556) {
            vec_36_1556 = &vec_36_1556_data;
        }
        /* vec_63_375 */
        if (!vec_63_375) {
            vec_63_375 = &vec_63_375_data;
        }
        /* vec_63_376 */
        if (!vec_63_376) {
            vec_63_376 = &vec_63_376_data;
        }
        /* vec_60_124 */
        if (!vec_60_124) {
            vec_60_124 = &vec_60_124_data;
        }
        /* vec_60_125 */
        if (!vec_60_125) {
            vec_60_125 = &vec_60_125_data;
        }
        /* vec_60_130 */
        if (!vec_60_130) {
            vec_60_130 = &vec_60_130_data;
        }
        /* vec_60_131 */
        if (!vec_60_131) {
            vec_60_131 = &vec_60_131_data;
        }
        /* vec_60_136 */
        if (!vec_60_136) {
            vec_60_136 = &vec_60_136_data;
        }
        /* vec_60_137 */
        if (!vec_60_137) {
            vec_60_137 = &vec_60_137_data;
        }
        /* vec_60_138 */
        if (!vec_60_138) {
            vec_60_138 = &vec_60_138_data;
        }
        /* vec_36_1497 */
        if (!vec_36_1497) {
            vec_36_1497 = &vec_36_1497_data;
        }
        /* vec_36_1498 */
        if (!vec_36_1498) {
            vec_36_1498 = &vec_36_1498_data;
        }
        /* vec_36_1499 */
        if (!vec_36_1499) {
            vec_36_1499 = &vec_36_1499_data;
        }
        /* vec_68_1019 */
        if (!vec_68_1019) {
            vec_68_1019 = &vec_68_1019_data;
        }
        /* vec_68_1020 */
        if (!vec_68_1020) {
            vec_68_1020 = &vec_68_1020_data;
        }
        /* vec_68_1007 */
        if (!vec_68_1007) {
            vec_68_1007 = &vec_68_1007_data;
        }
        /* vec_68_1008 */
        if (!vec_68_1008) {
            vec_68_1008 = &vec_68_1008_data;
        }
        /* vec_36_1522 */
        if (!vec_36_1522) {
            vec_36_1522 = &vec_36_1522_data;
        }
        /* vec_36_1523 */
        if (!vec_36_1523) {
            vec_36_1523 = &vec_36_1523_data;
        }
        /* vec_36_1524 */
        if (!vec_36_1524) {
            vec_36_1524 = &vec_36_1524_data;
        }
        /* vec_36_1525 */
        if (!vec_36_1525) {
            vec_36_1525 = &vec_36_1525_data;
        }
        /* vec_36_1526 */
        if (!vec_36_1526) {
            vec_36_1526 = &vec_36_1526_data;
        }
        /* vec_36_1530 */
        if (!vec_36_1530) {
            vec_36_1530 = &vec_36_1530_data;
        }
        /* vec_36_1531 */
        if (!vec_36_1531) {
            vec_36_1531 = &vec_36_1531_data;
        }
        /* vec_36_1532 */
        if (!vec_36_1532) {
            vec_36_1532 = &vec_36_1532_data;
        }
        /* vec_36_1533 */
        if (!vec_36_1533) {
            vec_36_1533 = &vec_36_1533_data;
        }
        /* vec_36_1534 */
        if (!vec_36_1534) {
            vec_36_1534 = &vec_36_1534_data;
        }
        /* vec_63_369 */
        if (!vec_63_369) {
            vec_63_369 = &vec_63_369_data;
        }
        /* vec_63_370 */
        if (!vec_63_370) {
            vec_63_370 = &vec_63_370_data;
        }
        /* vec_60_104 */
        if (!vec_60_104) {
            vec_60_104 = &vec_60_104_data;
        }
        /* vec_60_105 */
        if (!vec_60_105) {
            vec_60_105 = &vec_60_105_data;
        }
        /* vec_60_110 */
        if (!vec_60_110) {
            vec_60_110 = &vec_60_110_data;
        }
        /* vec_60_111 */
        if (!vec_60_111) {
            vec_60_111 = &vec_60_111_data;
        }
        /* vec_60_112 */
        if (!vec_60_112) {
            vec_60_112 = &vec_60_112_data;
        }
        /* vec_60_3 */
        if (!vec_60_3) {
            vec_60_3 = &vec_60_3_data;
        }
        /* vec_60_12 */
        if (!vec_60_12) {
            vec_60_12 = &vec_60_12_data;
        }
        /* vec_60_82 */
        if (!vec_60_82) {
            vec_60_82 = &vec_60_82_data;
        }
        /* vec_60_83 */
        if (!vec_60_83) {
            vec_60_83 = &vec_60_83_data;
        }
        /* vec_60_88 */
        if (!vec_60_88) {
            vec_60_88 = &vec_60_88_data;
        }
        /* vec_56_1838 */
        if (!vec_56_1838) {
            vec_56_1838 = &vec_56_1838_data;
        }
        /* vec_56_1839 */
        if (!vec_56_1839) {
            vec_56_1839 = &vec_56_1839_data;
        }
        /* vec_56_1840 */
        if (!vec_56_1840) {
            vec_56_1840 = &vec_56_1840_data;
        }
        /* vec_56_1841 */
        if (!vec_56_1841) {
            vec_56_1841 = &vec_56_1841_data;
        }
        /* vec_56_1842 */
        if (!vec_56_1842) {
            vec_56_1842 = &vec_56_1842_data;
        }
        /* vec_56_1846 */
        if (!vec_56_1846) {
            vec_56_1846 = &vec_56_1846_data;
        }
        /* vec_56_1847 */
        if (!vec_56_1847) {
            vec_56_1847 = &vec_56_1847_data;
        }
        /* vec_56_1848 */
        if (!vec_56_1848) {
            vec_56_1848 = &vec_56_1848_data;
        }
        /* vec_56_1849 */
        if (!vec_56_1849) {
            vec_56_1849 = &vec_56_1849_data;
        }
        /* vec_56_1850 */
        if (!vec_56_1850) {
            vec_56_1850 = &vec_56_1850_data;
        }
        /* vec_60_294 */
        if (!vec_60_294) {
            vec_60_294 = &vec_60_294_data;
        }
        /* vec_60_299 */
        if (!vec_60_299) {
            vec_60_299 = &vec_60_299_data;
        }
        /* vec_60_304 */
        if (!vec_60_304) {
            vec_60_304 = &vec_60_304_data;
        }
        /* vec_60_305 */
        if (!vec_60_305) {
            vec_60_305 = &vec_60_305_data;
        }
        /* vec_60_310 */
        if (!vec_60_310) {
            vec_60_310 = &vec_60_310_data;
        }
        /* vec_60_317 */
        if (!vec_60_317) {
            vec_60_317 = &vec_60_317_data;
        }
        /* vec_60_318 */
        if (!vec_60_318) {
            vec_60_318 = &vec_60_318_data;
        }
        /* vec_60_319 */
        if (!vec_60_319) {
            vec_60_319 = &vec_60_319_data;
        }
        /* vec_60_320 */
        if (!vec_60_320) {
            vec_60_320 = &vec_60_320_data;
        }
        /* vec_60_321 */
        if (!vec_60_321) {
            vec_60_321 = &vec_60_321_data;
        }
        /* vec_60_325 */
        if (!vec_60_325) {
            vec_60_325 = &vec_60_325_data;
        }
        /* vec_60_326 */
        if (!vec_60_326) {
            vec_60_326 = &vec_60_326_data;
        }
        /* vec_60_327 */
        if (!vec_60_327) {
            vec_60_327 = &vec_60_327_data;
        }
        /* vec_60_328 */
        if (!vec_60_328) {
            vec_60_328 = &vec_60_328_data;
        }
        /* vec_60_329 */
        if (!vec_60_329) {
            vec_60_329 = &vec_60_329_data;
        }
        /* vec_60_348 */
        if (!vec_60_348) {
            vec_60_348 = &vec_60_348_data;
        }
        /* vec_60_349 */
        if (!vec_60_349) {
            vec_60_349 = &vec_60_349_data;
        }
        /* vec_60_350 */
        if (!vec_60_350) {
            vec_60_350 = &vec_60_350_data;
        }
        /* vec_63_363 */
        if (!vec_63_363) {
            vec_63_363 = &vec_63_363_data;
        }
        /* vec_63_364 */
        if (!vec_63_364) {
            vec_63_364 = &vec_63_364_data;
        }
        /* vec_63_365 */
        if (!vec_63_365) {
            vec_63_365 = &vec_63_365_data;
        }
        /* vec_63_418 */
        if (!vec_63_418) {
            vec_63_418 = &vec_63_418_data;
        }
        /* vec_63_419 */
        if (!vec_63_419) {
            vec_63_419 = &vec_63_419_data;
        }
        /* vec_63_420 */
        if (!vec_63_420) {
            vec_63_420 = &vec_63_420_data;
        }
        /* vec_63_421 */
        if (!vec_63_421) {
            vec_63_421 = &vec_63_421_data;
        }
        /* vec_63_422 */
        if (!vec_63_422) {
            vec_63_422 = &vec_63_422_data;
        }
        /* vec_63_426 */
        if (!vec_63_426) {
            vec_63_426 = &vec_63_426_data;
        }
        /* vec_63_427 */
        if (!vec_63_427) {
            vec_63_427 = &vec_63_427_data;
        }
        /* vec_63_428 */
        if (!vec_63_428) {
            vec_63_428 = &vec_63_428_data;
        }
        /* vec_63_429 */
        if (!vec_63_429) {
            vec_63_429 = &vec_63_429_data;
        }
        /* vec_63_430 */
        if (!vec_63_430) {
            vec_63_430 = &vec_63_430_data;
        }
        /* vec_63_834 */
        if (!vec_63_834) {
            vec_63_834 = &vec_63_834_data;
        }
        /* vec_63_835 */
        if (!vec_63_835) {
            vec_63_835 = &vec_63_835_data;
        }
        /* vec_63_840 */
        if (!vec_63_840) {
            vec_63_840 = &vec_63_840_data;
        }
        /* vec_63_841 */
        if (!vec_63_841) {
            vec_63_841 = &vec_63_841_data;
        }
        /* vec_63_450 */
        if (!vec_63_450) {
            vec_63_450 = &vec_63_450_data;
        }
        /* vec_63_451 */
        if (!vec_63_451) {
            vec_63_451 = &vec_63_451_data;
        }
        /* vec_63_456 */
        if (!vec_63_456) {
            vec_63_456 = &vec_63_456_data;
        }
        /* vec_63_457 */
        if (!vec_63_457) {
            vec_63_457 = &vec_63_457_data;
        }
        /* vec_63_458 */
        if (!vec_63_458) {
            vec_63_458 = &vec_63_458_data;
        }
        /* vec_63_667 */
        if (!vec_63_667) {
            vec_63_667 = &vec_63_667_data;
        }
        /* vec_63_668 */
        if (!vec_63_668) {
            vec_63_668 = &vec_63_668_data;
        }
        /* vec_63_669 */
        if (!vec_63_669) {
            vec_63_669 = &vec_63_669_data;
        }
        /* vec_63_673 */
        if (!vec_63_673) {
            vec_63_673 = &vec_63_673_data;
        }
        /* vec_63_674 */
        if (!vec_63_674) {
            vec_63_674 = &vec_63_674_data;
        }
        /* vec_63_687 */
        if (!vec_63_687) {
            vec_63_687 = &vec_63_687_data;
        }
        /* vec_63_688 */
        if (!vec_63_688) {
            vec_63_688 = &vec_63_688_data;
        }
        /* vec_63_689 */
        if (!vec_63_689) {
            vec_63_689 = &vec_63_689_data;
        }
        /* icon_arrow_left */
        if (!icon_arrow_left) {
            icon_arrow_left = &icon_arrow_left_data;
        }
        /* icon_check */
        if (!icon_check) {
            icon_check = &icon_check_data;
        }
        /* icon_chevron_down */
        if (!icon_chevron_down) {
            icon_chevron_down = &icon_chevron_down_data;
        }
        /* icon_chevron_up */
        if (!icon_chevron_up) {
            icon_chevron_up = &icon_chevron_up_data;
        }
        /* icon_close */
        if (!icon_close) {
            icon_close = &icon_close_data;
        }
        /* icon_gender_male */
        if (!icon_gender_male) {
            icon_gender_male = &icon_gender_male_data;
        }
        /* icon_gender_female */
        if (!icon_gender_female) {
            icon_gender_female = &icon_gender_female_data;
        }
        /* icon_monitor */
        if (!icon_monitor) {
            icon_monitor = &icon_monitor_data;
        }
        /* icon_priority_immediate */
        if (!icon_priority_immediate) {
            icon_priority_immediate = &icon_priority_immediate_data;
        }
        /* icon_priority_delayed */
        if (!icon_priority_delayed) {
            icon_priority_delayed = &icon_priority_delayed_data;
        }
        /* icon_priority_minor */
        if (!icon_priority_minor) {
            icon_priority_minor = &icon_priority_minor_data;
        }
        /* icon_vital_spo2_sm */
        if (!icon_vital_spo2_sm) {
            icon_vital_spo2_sm = &icon_vital_spo2_sm_data;
        }
        /* icon_vital_hr_sm */
        if (!icon_vital_hr_sm) {
            icon_vital_hr_sm = &icon_vital_hr_sm_data;
        }
        /* icon_vital_rr_sm */
        if (!icon_vital_rr_sm) {
            icon_vital_rr_sm = &icon_vital_rr_sm_data;
        }
        /* icon_vital_bp_sm */
        if (!icon_vital_bp_sm) {
            icon_vital_bp_sm = &icon_vital_bp_sm_data;
        }
        /* icon_update */
        if (!icon_update) {
            icon_update = &icon_update_data;
        }
        /* icon_heart_pulse */
        if (!icon_heart_pulse) {
            icon_heart_pulse = &icon_heart_pulse_data;
        }
        /* icon_oxygen */
        if (!icon_oxygen) {
            icon_oxygen = &icon_oxygen_data;
        }
        /* icon_respiratory */
        if (!icon_respiratory) {
            icon_respiratory = &icon_respiratory_data;
        }
        /* icon_blood_pressure */
        if (!icon_blood_pressure) {
            icon_blood_pressure = &icon_blood_pressure_data;
        }
        /* icon_menu */
        if (!icon_menu) {
            icon_menu = &icon_menu_data;
        }
        /* icon_pause */
        if (!icon_pause) {
            icon_pause = &icon_pause_data;
        }
        /* icon_play */
        if (!icon_play) {
            icon_play = &icon_play_data;
        }
        /* icon_power */
        if (!icon_power) {
            icon_power = &icon_power_data;
        }
        /* icon_refresh */
        if (!icon_refresh) {
            icon_refresh = &icon_refresh_data;
        }
        /* icon_search */
        if (!icon_search) {
            icon_search = &icon_search_data;
        }
        /* icon_rfid_scan */
        if (!icon_rfid_scan) {
            icon_rfid_scan = &icon_rfid_scan_data;
        }
        /* icon_rfid_scan_lg */
        if (!icon_rfid_scan_lg) {
            icon_rfid_scan_lg = &icon_rfid_scan_lg_data;
        }
        /* icon_signal */
        if (!icon_signal) {
            icon_signal = &icon_signal_data;
        }
        /* icon_star */
        if (!icon_star) {
            icon_star = &icon_star_data;
        }
        /* icon_wifi_high */
        if (!icon_wifi_high) {
            icon_wifi_high = &icon_wifi_high_data;
        }
        /* icon_wifi_low */
        if (!icon_wifi_low) {
            icon_wifi_low = &icon_wifi_low_data;
        }
        /* icon_wifi_zero */
        if (!icon_wifi_zero) {
            icon_wifi_zero = &icon_wifi_zero_data;
        }
        /* logo_light_for_dark */
        if (!logo_light_for_dark) {
            logo_light_for_dark = &logo_light_for_dark_data;
        }
        /* battery_empty */
        if (!battery_empty) {
            battery_empty = &battery_empty_data;
        }
        /* battery_medium */
        if (!battery_medium) {
            battery_medium = &battery_medium_data;
        }
        /* battery_full */
        if (!battery_full) {
            battery_full = &battery_full_data;
        }
        /* battery_charging */
        if (!battery_charging) {
            battery_charging = &battery_charging_data;
        }
    }
    #endif

    /*----------------
     * Global styles
     *----------------*/

    static bool style_inited = false;

    if (!style_inited) {
        /*Init all styles*/
        lv_style_init(&shadow_button);
        lv_style_init(&shadow_icon);
        lv_style_init(&screen_base);
        lv_style_init(&style_screen_light);
        lv_style_init(&style_screen_dark);
        lv_style_init(&style_panel_light);
        lv_style_init(&style_panel_dark);
        lv_style_init(&style_text_accent);
        lv_style_init(&style_text_muted);
        lv_style_init(&style_scrollbar);
        lv_style_init(&style_buttonbar_cell_grad);

        lv_style_set_shadow_width(&shadow_button, 20);
        lv_style_set_shadow_offset_y(&shadow_button, 16);
        lv_style_set_shadow_spread(&shadow_button, -8);
        lv_style_set_shadow_color(&shadow_button, lv_color_hex(0x0C0C0D));
        lv_style_set_shadow_opa(&shadow_button, 102);
        lv_style_set_shadow_width(&shadow_icon, 4);
        lv_style_set_shadow_offset_y(&shadow_icon, 4);
        lv_style_set_shadow_color(&shadow_icon, lv_color_hex(0x000000));
        lv_style_set_shadow_opa(&shadow_icon, 128);
        lv_style_set_border_width(&screen_base, 0);
        lv_style_set_radius(&screen_base, 0);
        lv_style_set_shadow_width(&screen_base, 0);
        lv_style_set_shadow_opa(&screen_base, 0);
        lv_style_set_bg_color(&style_screen_light, COLOR_LIGHT_BG);
        lv_style_set_bg_opa(&style_screen_light, (255 * 100 / 100));
        lv_style_set_text_color(&style_screen_light, COLOR_LIGHT_TEXT);
        lv_style_set_text_font(&style_screen_light, font_body);
        lv_style_set_bg_color(&style_screen_dark, COLOR_DARK_BG);
        lv_style_set_bg_opa(&style_screen_dark, (255 * 100 / 100));
        lv_style_set_text_color(&style_screen_dark, COLOR_DARK_TEXT);
        lv_style_set_text_font(&style_screen_dark, font_body);
        lv_style_set_bg_color(&style_panel_light, COLOR_LIGHT_PANEL);
        lv_style_set_bg_opa(&style_panel_light, (255 * 100 / 100));
        lv_style_set_border_color(&style_panel_light, COLOR_LIGHT_TEXT);
        lv_style_set_border_opa(&style_panel_light, (255 * 20 / 100));
        lv_style_set_border_width(&style_panel_light, BORDER_WIDTH);
        lv_style_set_text_color(&style_panel_light, COLOR_LIGHT_TEXT);
        lv_style_set_pad_all(&style_panel_light, SPACE_MD);
        lv_style_set_radius(&style_panel_light, RADIUS_DEFAULT);
        lv_style_set_bg_color(&style_panel_dark, COLOR_DARK_PANEL);
        lv_style_set_bg_opa(&style_panel_dark, (255 * 100 / 100));
        lv_style_set_border_color(&style_panel_dark, COLOR_DARK_PANEL);
        lv_style_set_border_opa(&style_panel_dark, (255 * 20 / 100));
        lv_style_set_border_width(&style_panel_dark, BORDER_WIDTH);
        lv_style_set_text_color(&style_panel_dark, COLOR_DARK_TEXT);
        lv_style_set_pad_all(&style_panel_dark, SPACE_MD);
        lv_style_set_radius(&style_panel_dark, RADIUS_DEFAULT);
        lv_style_set_text_color(&style_text_accent, COLOR_ACCENT);
        lv_style_set_text_color(&style_text_muted, COLOR_TEXT_SECONDARY);
        lv_style_set_width(&style_scrollbar, 0);
        lv_style_set_bg_opa(&style_scrollbar, (255 * 0 / 100));
        lv_style_set_border_width(&style_scrollbar, 0);
        lv_style_set_pad_all(&style_scrollbar, 0);
        lv_style_set_bg_color(&style_buttonbar_cell_grad, COLOR_BUTTONBAR_TOP);
        lv_style_set_bg_grad_color(&style_buttonbar_cell_grad, COLOR_BUTTONBAR_BOTTOM);
        lv_style_set_bg_grad_dir(&style_buttonbar_cell_grad, LV_GRAD_DIR_VER);
        lv_style_set_bg_opa(&style_buttonbar_cell_grad, (255 * 100 / 100));
        lv_style_set_border_color(&style_buttonbar_cell_grad, COLOR_DARK_PANEL);
        lv_style_set_border_width(&style_buttonbar_cell_grad, 1);
        lv_style_set_border_opa(&style_buttonbar_cell_grad, (255 * 100 / 100));
        lv_style_set_border_side(&style_buttonbar_cell_grad, LV_BORDER_SIDE_FULL);
        lv_style_set_outline_width(&style_buttonbar_cell_grad, 0);
        lv_style_set_outline_opa(&style_buttonbar_cell_grad, (255 * 0 / 100));
        lv_style_set_shadow_width(&style_buttonbar_cell_grad, 0);
        lv_style_set_radius(&style_buttonbar_cell_grad, 0);

        style_inited = true;
    }

    /*----------------
     * Subjects
     *----------------*/
    lv_subject_init_int(&subject_theme_dark, 1);
    lv_subject_set_min_value_int(&subject_theme_dark, 0);
    lv_subject_set_max_value_int(&subject_theme_dark, 1);
    lv_subject_init_int(&subject_brightness, 60);
    lv_subject_set_min_value_int(&subject_brightness, 0);
    lv_subject_set_max_value_int(&subject_brightness, 100);
    lv_subject_init_int(&subject_show_keyboard, 0);

    /*----------------
     * Translations
     *----------------*/

    #ifndef LV_EDITOR_PREVIEW
        lv_translation_add_static(translation_languages, translation_tags, translation_texts);
        lv_translation_set_language(translation_languages[0]);
    #endif

#if defined(LV_USE_XML) && LV_USE_XML
    /* Register widgets */

    /* Check all fonts / default if needed. This prevents fonts that are used in one target but
       defined in another from causing assertion failures during rendering of the Preview. */
    check_font(&font_inter_regular_36, "font_inter_regular_36");
    check_font(&font_inter_semi_bold_20, "font_inter_semi_bold_20");
    check_font(&font_inter_semi_bold_16, "font_inter_semi_bold_16");
    check_font(&font_inter_bold_32, "font_inter_bold_32");
    check_font(&font_inter_medium_16, "font_inter_medium_16");
    check_font(&font_inter_semi_bold_36, "font_inter_semi_bold_36");
    check_font(&font_inter_bold_48, "font_inter_bold_48");
    check_font(&font_inter_semi_bold_18, "font_inter_semi_bold_18");
    check_font(&font_inter_regular_14, "font_inter_regular_14");
    check_font(&font_inter_regular_13, "font_inter_regular_13");
    check_font(&font_inter_semi_bold_13, "font_inter_semi_bold_13");
    check_font(&font_inter_regular_10, "font_inter_regular_10");
    check_font(&font_inter_regular_16, "font_inter_regular_16");
    check_font(&font_poppins_semibold_35_65217590332031, "font_poppins_semibold_35_65217590332031");
    check_font(&font_inter_semi_bold_24, "font_inter_semi_bold_24");
    check_font(&font_inter_regular_12, "font_inter_regular_12");
    check_font(&font_inter_bold_24, "font_inter_bold_24");
    check_font(&font_inter_bold_20, "font_inter_bold_20");
    check_font(&font_inter_bold_18, "font_inter_bold_18");
    check_font(&font_inter_regular_9, "font_inter_regular_9");
    check_font(&font_inter_semi_bold_14, "font_inter_semi_bold_14");
    check_font(&font_inter_bold_56, "font_inter_bold_56");
    check_font(&font_inter_regular_24, "font_inter_regular_24");
    check_font(&font_inter_semi_bold_12, "font_inter_semi_bold_12");
    check_font(&font_inter_bold_36, "font_inter_bold_36");
    check_font(&font_inter_regular_11, "font_inter_regular_11");
    check_font(&font_inter_bold_28, "font_inter_bold_28");
    check_font(&font_inter_bold_47, "font_inter_bold_47");
    check_font(&font_inter_regular_47, "font_inter_regular_47");
    check_font(&font_helvetica_regular_7_000000476837158, "font_helvetica_regular_7_000000476837158");
    check_font(&font_inter_regular_12_000000953674316, "font_inter_regular_12_000000953674316");
    check_font(&font_inter_bold_30, "font_inter_bold_30");
    check_font(&font_inter_medium_14_000000953674316, "font_inter_medium_14_000000953674316");
    check_font(&font_inter_bold_14, "font_inter_bold_14");
    check_font(&font_inter_regular_15, "font_inter_regular_15");
    check_font(&font_inter_medium_13, "font_inter_medium_13");
    check_font(&font_inter_medium_12, "font_inter_medium_12");
    check_font(&font_inter_bold_12, "font_inter_bold_12");
    check_font(&font_inter_medium_15, "font_inter_medium_15");
    check_font(&font_inter_bold_14_000000953674316, "font_inter_bold_14_000000953674316");
    check_font(&font_inter_extra_bold_18, "font_inter_extra_bold_18");
    check_font(&font_inter_medium_18, "font_inter_medium_18");
    check_font(&font_inter_medium_14, "font_inter_medium_14");
    check_font(&font_inter_regular_18, "font_inter_regular_18");
    check_font(&font_inter_bold_25, "font_inter_bold_25");
    check_font(&font_inter_medium_13_999999046325684, "font_inter_medium_13_999999046325684");
    check_font(&font_inter_extra_bold_20, "font_inter_extra_bold_20");
    check_font(&font_inter_bold_35, "font_inter_bold_35");
    check_font(&font_inter_extra_bold_25, "font_inter_extra_bold_25");
    check_font(&font_inter_medium_10, "font_inter_medium_10");
    check_font(&font_body_symbols, "font_body_symbols");
    check_font(&font_body, "font_body");
    check_font(&font_h5, "font_h5");
    check_font(&font_h4, "font_h4");
    check_font(&font_h3, "font_h3");
    check_font(&font_h2, "font_h2");
    check_font(&font_h1, "font_h1");

    /* Register fonts */
    lv_xml_register_font(NULL, "font_inter_regular_36", font_inter_regular_36);
    lv_xml_register_font(NULL, "font_inter_semi_bold_20", font_inter_semi_bold_20);
    lv_xml_register_font(NULL, "font_inter_semi_bold_16", font_inter_semi_bold_16);
    lv_xml_register_font(NULL, "font_inter_bold_32", font_inter_bold_32);
    lv_xml_register_font(NULL, "font_inter_medium_16", font_inter_medium_16);
    lv_xml_register_font(NULL, "font_inter_semi_bold_36", font_inter_semi_bold_36);
    lv_xml_register_font(NULL, "font_inter_bold_48", font_inter_bold_48);
    lv_xml_register_font(NULL, "font_inter_semi_bold_18", font_inter_semi_bold_18);
    lv_xml_register_font(NULL, "font_inter_regular_14", font_inter_regular_14);
    lv_xml_register_font(NULL, "font_inter_regular_13", font_inter_regular_13);
    lv_xml_register_font(NULL, "font_inter_semi_bold_13", font_inter_semi_bold_13);
    lv_xml_register_font(NULL, "font_inter_regular_10", font_inter_regular_10);
    lv_xml_register_font(NULL, "font_inter_regular_16", font_inter_regular_16);
    lv_xml_register_font(NULL, "font_poppins_semibold_35_65217590332031", font_poppins_semibold_35_65217590332031);
    lv_xml_register_font(NULL, "font_inter_semi_bold_24", font_inter_semi_bold_24);
    lv_xml_register_font(NULL, "font_inter_regular_12", font_inter_regular_12);
    lv_xml_register_font(NULL, "font_inter_bold_24", font_inter_bold_24);
    lv_xml_register_font(NULL, "font_inter_bold_20", font_inter_bold_20);
    lv_xml_register_font(NULL, "font_inter_bold_18", font_inter_bold_18);
    lv_xml_register_font(NULL, "font_inter_regular_9", font_inter_regular_9);
    lv_xml_register_font(NULL, "font_inter_semi_bold_14", font_inter_semi_bold_14);
    lv_xml_register_font(NULL, "font_inter_bold_56", font_inter_bold_56);
    lv_xml_register_font(NULL, "font_inter_regular_24", font_inter_regular_24);
    lv_xml_register_font(NULL, "font_inter_semi_bold_12", font_inter_semi_bold_12);
    lv_xml_register_font(NULL, "font_inter_bold_36", font_inter_bold_36);
    lv_xml_register_font(NULL, "font_inter_regular_11", font_inter_regular_11);
    lv_xml_register_font(NULL, "font_inter_bold_28", font_inter_bold_28);
    lv_xml_register_font(NULL, "font_inter_bold_47", font_inter_bold_47);
    lv_xml_register_font(NULL, "font_inter_regular_47", font_inter_regular_47);
    lv_xml_register_font(NULL, "font_helvetica_regular_7_000000476837158", font_helvetica_regular_7_000000476837158);
    lv_xml_register_font(NULL, "font_inter_regular_12_000000953674316", font_inter_regular_12_000000953674316);
    lv_xml_register_font(NULL, "font_inter_bold_30", font_inter_bold_30);
    lv_xml_register_font(NULL, "font_inter_medium_14_000000953674316", font_inter_medium_14_000000953674316);
    lv_xml_register_font(NULL, "font_inter_bold_14", font_inter_bold_14);
    lv_xml_register_font(NULL, "font_inter_regular_15", font_inter_regular_15);
    lv_xml_register_font(NULL, "font_inter_medium_13", font_inter_medium_13);
    lv_xml_register_font(NULL, "font_inter_medium_12", font_inter_medium_12);
    lv_xml_register_font(NULL, "font_inter_bold_12", font_inter_bold_12);
    lv_xml_register_font(NULL, "font_inter_medium_15", font_inter_medium_15);
    lv_xml_register_font(NULL, "font_inter_bold_14_000000953674316", font_inter_bold_14_000000953674316);
    lv_xml_register_font(NULL, "font_inter_extra_bold_18", font_inter_extra_bold_18);
    lv_xml_register_font(NULL, "font_inter_medium_18", font_inter_medium_18);
    lv_xml_register_font(NULL, "font_inter_medium_14", font_inter_medium_14);
    lv_xml_register_font(NULL, "font_inter_regular_18", font_inter_regular_18);
    lv_xml_register_font(NULL, "font_inter_bold_25", font_inter_bold_25);
    lv_xml_register_font(NULL, "font_inter_medium_13_999999046325684", font_inter_medium_13_999999046325684);
    lv_xml_register_font(NULL, "font_inter_extra_bold_20", font_inter_extra_bold_20);
    lv_xml_register_font(NULL, "font_inter_bold_35", font_inter_bold_35);
    lv_xml_register_font(NULL, "font_inter_extra_bold_25", font_inter_extra_bold_25);
    lv_xml_register_font(NULL, "font_inter_medium_10", font_inter_medium_10);
    lv_xml_register_font(NULL, "font_body_symbols", font_body_symbols);
    lv_xml_register_font(NULL, "font_body", font_body);
    lv_xml_register_font(NULL, "font_h5", font_h5);
    lv_xml_register_font(NULL, "font_h4", font_h4);
    lv_xml_register_font(NULL, "font_h3", font_h3);
    lv_xml_register_font(NULL, "font_h2", font_h2);
    lv_xml_register_font(NULL, "font_h1", font_h1);

    /* Register subjects */
    lv_xml_register_subject(NULL, "subject_theme_dark", &subject_theme_dark);
    lv_xml_register_subject(NULL, "subject_brightness", &subject_brightness);
    lv_xml_register_subject(NULL, "subject_show_keyboard", &subject_show_keyboard);

    /* Register callbacks */
#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if defined(LV_USE_XML) && LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    /* Register images */
    lv_xml_register_image(NULL, "icon_161221", icon_161221);
    lv_xml_register_image(NULL, "icon_361488", icon_361488);
    lv_xml_register_image(NULL, "icon_63636", icon_63636);
    lv_xml_register_image(NULL, "icon_63640", icon_63640);
    lv_xml_register_image(NULL, "icon_63653", icon_63653);
    lv_xml_register_image(NULL, "icon_63657", icon_63657);
    lv_xml_register_image(NULL, "vec_36_1610", vec_36_1610);
    lv_xml_register_image(NULL, "vec_36_1611", vec_36_1611);
    lv_xml_register_image(NULL, "vec_36_1612", vec_36_1612);
    lv_xml_register_image(NULL, "vec_36_1613", vec_36_1613);
    lv_xml_register_image(NULL, "vec_36_1614", vec_36_1614);
    lv_xml_register_image(NULL, "vec_36_1618", vec_36_1618);
    lv_xml_register_image(NULL, "vec_36_1619", vec_36_1619);
    lv_xml_register_image(NULL, "vec_36_1620", vec_36_1620);
    lv_xml_register_image(NULL, "vec_36_1621", vec_36_1621);
    lv_xml_register_image(NULL, "vec_36_1622", vec_36_1622);
    lv_xml_register_image(NULL, "vec_60_215", vec_60_215);
    lv_xml_register_image(NULL, "vec_60_216", vec_60_216);
    lv_xml_register_image(NULL, "vec_60_217", vec_60_217);
    lv_xml_register_image(NULL, "vec_60_218", vec_60_218);
    lv_xml_register_image(NULL, "vec_60_219", vec_60_219);
    lv_xml_register_image(NULL, "vec_60_224", vec_60_224);
    lv_xml_register_image(NULL, "vec_60_225", vec_60_225);
    lv_xml_register_image(NULL, "vec_60_230", vec_60_230);
    lv_xml_register_image(NULL, "vec_60_231", vec_60_231);
    lv_xml_register_image(NULL, "vec_60_232", vec_60_232);
    lv_xml_register_image(NULL, "vec_282_31", vec_282_31);
    lv_xml_register_image(NULL, "vec_282_32", vec_282_32);
    lv_xml_register_image(NULL, "vec_282_33", vec_282_33);
    lv_xml_register_image(NULL, "vec_282_35", vec_282_35);
    lv_xml_register_image(NULL, "vec_16_385", vec_16_385);
    lv_xml_register_image(NULL, "vec_16_386", vec_16_386);
    lv_xml_register_image(NULL, "vec_16_387", vec_16_387);
    lv_xml_register_image(NULL, "vec_16_388", vec_16_388);
    lv_xml_register_image(NULL, "vec_16_389", vec_16_389);
    lv_xml_register_image(NULL, "vec_36_1588", vec_36_1588);
    lv_xml_register_image(NULL, "vec_36_1589", vec_36_1589);
    lv_xml_register_image(NULL, "vec_36_1590", vec_36_1590);
    lv_xml_register_image(NULL, "vec_36_1591", vec_36_1591);
    lv_xml_register_image(NULL, "vec_36_1592", vec_36_1592);
    lv_xml_register_image(NULL, "vec_36_1596", vec_36_1596);
    lv_xml_register_image(NULL, "vec_36_1597", vec_36_1597);
    lv_xml_register_image(NULL, "vec_36_1598", vec_36_1598);
    lv_xml_register_image(NULL, "vec_36_1599", vec_36_1599);
    lv_xml_register_image(NULL, "vec_36_1600", vec_36_1600);
    lv_xml_register_image(NULL, "vec_60_271", vec_60_271);
    lv_xml_register_image(NULL, "vec_60_272", vec_60_272);
    lv_xml_register_image(NULL, "vec_60_278", vec_60_278);
    lv_xml_register_image(NULL, "vec_60_279", vec_60_279);
    lv_xml_register_image(NULL, "vec_60_284", vec_60_284);
    lv_xml_register_image(NULL, "vec_60_285", vec_60_285);
    lv_xml_register_image(NULL, "vec_60_286", vec_60_286);
    lv_xml_register_image(NULL, "vec_16_583", vec_16_583);
    lv_xml_register_image(NULL, "vec_16_584", vec_16_584);
    lv_xml_register_image(NULL, "vec_16_585", vec_16_585);
    lv_xml_register_image(NULL, "vec_16_586", vec_16_586);
    lv_xml_register_image(NULL, "vec_16_587", vec_16_587);
    lv_xml_register_image(NULL, "vec_36_1566", vec_36_1566);
    lv_xml_register_image(NULL, "vec_36_1567", vec_36_1567);
    lv_xml_register_image(NULL, "vec_36_1568", vec_36_1568);
    lv_xml_register_image(NULL, "vec_36_1569", vec_36_1569);
    lv_xml_register_image(NULL, "vec_36_1570", vec_36_1570);
    lv_xml_register_image(NULL, "vec_36_1574", vec_36_1574);
    lv_xml_register_image(NULL, "vec_36_1575", vec_36_1575);
    lv_xml_register_image(NULL, "vec_36_1576", vec_36_1576);
    lv_xml_register_image(NULL, "vec_36_1577", vec_36_1577);
    lv_xml_register_image(NULL, "vec_36_1578", vec_36_1578);
    lv_xml_register_image(NULL, "vec_60_145", vec_60_145);
    lv_xml_register_image(NULL, "vec_60_150", vec_60_150);
    lv_xml_register_image(NULL, "vec_60_151", vec_60_151);
    lv_xml_register_image(NULL, "vec_60_156", vec_60_156);
    lv_xml_register_image(NULL, "vec_60_157", vec_60_157);
    lv_xml_register_image(NULL, "vec_60_162", vec_60_162);
    lv_xml_register_image(NULL, "vec_60_163", vec_60_163);
    lv_xml_register_image(NULL, "vec_60_164", vec_60_164);
    lv_xml_register_image(NULL, "vec_16_1205", vec_16_1205);
    lv_xml_register_image(NULL, "vec_16_1206", vec_16_1206);
    lv_xml_register_image(NULL, "vec_16_1207", vec_16_1207);
    lv_xml_register_image(NULL, "vec_68_1023", vec_68_1023);
    lv_xml_register_image(NULL, "vec_68_1024", vec_68_1024);
    lv_xml_register_image(NULL, "vec_16_1235", vec_16_1235);
    lv_xml_register_image(NULL, "vec_16_1236", vec_16_1236);
    lv_xml_register_image(NULL, "vec_16_1237", vec_16_1237);
    lv_xml_register_image(NULL, "vec_68_865", vec_68_865);
    lv_xml_register_image(NULL, "vec_68_866", vec_68_866);
    lv_xml_register_image(NULL, "vec_36_1544", vec_36_1544);
    lv_xml_register_image(NULL, "vec_36_1545", vec_36_1545);
    lv_xml_register_image(NULL, "vec_36_1546", vec_36_1546);
    lv_xml_register_image(NULL, "vec_36_1547", vec_36_1547);
    lv_xml_register_image(NULL, "vec_36_1548", vec_36_1548);
    lv_xml_register_image(NULL, "vec_36_1552", vec_36_1552);
    lv_xml_register_image(NULL, "vec_36_1553", vec_36_1553);
    lv_xml_register_image(NULL, "vec_36_1554", vec_36_1554);
    lv_xml_register_image(NULL, "vec_36_1555", vec_36_1555);
    lv_xml_register_image(NULL, "vec_36_1556", vec_36_1556);
    lv_xml_register_image(NULL, "vec_63_375", vec_63_375);
    lv_xml_register_image(NULL, "vec_63_376", vec_63_376);
    lv_xml_register_image(NULL, "vec_60_124", vec_60_124);
    lv_xml_register_image(NULL, "vec_60_125", vec_60_125);
    lv_xml_register_image(NULL, "vec_60_130", vec_60_130);
    lv_xml_register_image(NULL, "vec_60_131", vec_60_131);
    lv_xml_register_image(NULL, "vec_60_136", vec_60_136);
    lv_xml_register_image(NULL, "vec_60_137", vec_60_137);
    lv_xml_register_image(NULL, "vec_60_138", vec_60_138);
    lv_xml_register_image(NULL, "vec_36_1497", vec_36_1497);
    lv_xml_register_image(NULL, "vec_36_1498", vec_36_1498);
    lv_xml_register_image(NULL, "vec_36_1499", vec_36_1499);
    lv_xml_register_image(NULL, "vec_68_1019", vec_68_1019);
    lv_xml_register_image(NULL, "vec_68_1020", vec_68_1020);
    lv_xml_register_image(NULL, "vec_68_1007", vec_68_1007);
    lv_xml_register_image(NULL, "vec_68_1008", vec_68_1008);
    lv_xml_register_image(NULL, "vec_36_1522", vec_36_1522);
    lv_xml_register_image(NULL, "vec_36_1523", vec_36_1523);
    lv_xml_register_image(NULL, "vec_36_1524", vec_36_1524);
    lv_xml_register_image(NULL, "vec_36_1525", vec_36_1525);
    lv_xml_register_image(NULL, "vec_36_1526", vec_36_1526);
    lv_xml_register_image(NULL, "vec_36_1530", vec_36_1530);
    lv_xml_register_image(NULL, "vec_36_1531", vec_36_1531);
    lv_xml_register_image(NULL, "vec_36_1532", vec_36_1532);
    lv_xml_register_image(NULL, "vec_36_1533", vec_36_1533);
    lv_xml_register_image(NULL, "vec_36_1534", vec_36_1534);
    lv_xml_register_image(NULL, "vec_63_369", vec_63_369);
    lv_xml_register_image(NULL, "vec_63_370", vec_63_370);
    lv_xml_register_image(NULL, "vec_60_104", vec_60_104);
    lv_xml_register_image(NULL, "vec_60_105", vec_60_105);
    lv_xml_register_image(NULL, "vec_60_110", vec_60_110);
    lv_xml_register_image(NULL, "vec_60_111", vec_60_111);
    lv_xml_register_image(NULL, "vec_60_112", vec_60_112);
    lv_xml_register_image(NULL, "vec_60_3", vec_60_3);
    lv_xml_register_image(NULL, "vec_60_12", vec_60_12);
    lv_xml_register_image(NULL, "vec_60_82", vec_60_82);
    lv_xml_register_image(NULL, "vec_60_83", vec_60_83);
    lv_xml_register_image(NULL, "vec_60_88", vec_60_88);
    lv_xml_register_image(NULL, "vec_56_1838", vec_56_1838);
    lv_xml_register_image(NULL, "vec_56_1839", vec_56_1839);
    lv_xml_register_image(NULL, "vec_56_1840", vec_56_1840);
    lv_xml_register_image(NULL, "vec_56_1841", vec_56_1841);
    lv_xml_register_image(NULL, "vec_56_1842", vec_56_1842);
    lv_xml_register_image(NULL, "vec_56_1846", vec_56_1846);
    lv_xml_register_image(NULL, "vec_56_1847", vec_56_1847);
    lv_xml_register_image(NULL, "vec_56_1848", vec_56_1848);
    lv_xml_register_image(NULL, "vec_56_1849", vec_56_1849);
    lv_xml_register_image(NULL, "vec_56_1850", vec_56_1850);
    lv_xml_register_image(NULL, "vec_60_294", vec_60_294);
    lv_xml_register_image(NULL, "vec_60_299", vec_60_299);
    lv_xml_register_image(NULL, "vec_60_304", vec_60_304);
    lv_xml_register_image(NULL, "vec_60_305", vec_60_305);
    lv_xml_register_image(NULL, "vec_60_310", vec_60_310);
    lv_xml_register_image(NULL, "vec_60_317", vec_60_317);
    lv_xml_register_image(NULL, "vec_60_318", vec_60_318);
    lv_xml_register_image(NULL, "vec_60_319", vec_60_319);
    lv_xml_register_image(NULL, "vec_60_320", vec_60_320);
    lv_xml_register_image(NULL, "vec_60_321", vec_60_321);
    lv_xml_register_image(NULL, "vec_60_325", vec_60_325);
    lv_xml_register_image(NULL, "vec_60_326", vec_60_326);
    lv_xml_register_image(NULL, "vec_60_327", vec_60_327);
    lv_xml_register_image(NULL, "vec_60_328", vec_60_328);
    lv_xml_register_image(NULL, "vec_60_329", vec_60_329);
    lv_xml_register_image(NULL, "vec_60_348", vec_60_348);
    lv_xml_register_image(NULL, "vec_60_349", vec_60_349);
    lv_xml_register_image(NULL, "vec_60_350", vec_60_350);
    lv_xml_register_image(NULL, "vec_63_363", vec_63_363);
    lv_xml_register_image(NULL, "vec_63_364", vec_63_364);
    lv_xml_register_image(NULL, "vec_63_365", vec_63_365);
    lv_xml_register_image(NULL, "vec_63_418", vec_63_418);
    lv_xml_register_image(NULL, "vec_63_419", vec_63_419);
    lv_xml_register_image(NULL, "vec_63_420", vec_63_420);
    lv_xml_register_image(NULL, "vec_63_421", vec_63_421);
    lv_xml_register_image(NULL, "vec_63_422", vec_63_422);
    lv_xml_register_image(NULL, "vec_63_426", vec_63_426);
    lv_xml_register_image(NULL, "vec_63_427", vec_63_427);
    lv_xml_register_image(NULL, "vec_63_428", vec_63_428);
    lv_xml_register_image(NULL, "vec_63_429", vec_63_429);
    lv_xml_register_image(NULL, "vec_63_430", vec_63_430);
    lv_xml_register_image(NULL, "vec_63_834", vec_63_834);
    lv_xml_register_image(NULL, "vec_63_835", vec_63_835);
    lv_xml_register_image(NULL, "vec_63_840", vec_63_840);
    lv_xml_register_image(NULL, "vec_63_841", vec_63_841);
    lv_xml_register_image(NULL, "vec_63_450", vec_63_450);
    lv_xml_register_image(NULL, "vec_63_451", vec_63_451);
    lv_xml_register_image(NULL, "vec_63_456", vec_63_456);
    lv_xml_register_image(NULL, "vec_63_457", vec_63_457);
    lv_xml_register_image(NULL, "vec_63_458", vec_63_458);
    lv_xml_register_image(NULL, "vec_63_667", vec_63_667);
    lv_xml_register_image(NULL, "vec_63_668", vec_63_668);
    lv_xml_register_image(NULL, "vec_63_669", vec_63_669);
    lv_xml_register_image(NULL, "vec_63_673", vec_63_673);
    lv_xml_register_image(NULL, "vec_63_674", vec_63_674);
    lv_xml_register_image(NULL, "vec_63_687", vec_63_687);
    lv_xml_register_image(NULL, "vec_63_688", vec_63_688);
    lv_xml_register_image(NULL, "vec_63_689", vec_63_689);
    lv_xml_register_image(NULL, "icon_arrow_left", icon_arrow_left);
    lv_xml_register_image(NULL, "icon_check", icon_check);
    lv_xml_register_image(NULL, "icon_chevron_down", icon_chevron_down);
    lv_xml_register_image(NULL, "icon_chevron_up", icon_chevron_up);
    lv_xml_register_image(NULL, "icon_close", icon_close);
    lv_xml_register_image(NULL, "icon_gender_male", icon_gender_male);
    lv_xml_register_image(NULL, "icon_gender_female", icon_gender_female);
    lv_xml_register_image(NULL, "icon_monitor", icon_monitor);
    lv_xml_register_image(NULL, "icon_priority_immediate", icon_priority_immediate);
    lv_xml_register_image(NULL, "icon_priority_delayed", icon_priority_delayed);
    lv_xml_register_image(NULL, "icon_priority_minor", icon_priority_minor);
    lv_xml_register_image(NULL, "icon_vital_spo2_sm", icon_vital_spo2_sm);
    lv_xml_register_image(NULL, "icon_vital_hr_sm", icon_vital_hr_sm);
    lv_xml_register_image(NULL, "icon_vital_rr_sm", icon_vital_rr_sm);
    lv_xml_register_image(NULL, "icon_vital_bp_sm", icon_vital_bp_sm);
    lv_xml_register_image(NULL, "icon_update", icon_update);
    lv_xml_register_image(NULL, "icon_heart_pulse", icon_heart_pulse);
    lv_xml_register_image(NULL, "icon_oxygen", icon_oxygen);
    lv_xml_register_image(NULL, "icon_respiratory", icon_respiratory);
    lv_xml_register_image(NULL, "icon_blood_pressure", icon_blood_pressure);
    lv_xml_register_image(NULL, "icon_menu", icon_menu);
    lv_xml_register_image(NULL, "icon_pause", icon_pause);
    lv_xml_register_image(NULL, "icon_play", icon_play);
    lv_xml_register_image(NULL, "icon_power", icon_power);
    lv_xml_register_image(NULL, "icon_refresh", icon_refresh);
    lv_xml_register_image(NULL, "icon_search", icon_search);
    lv_xml_register_image(NULL, "icon_rfid_scan", icon_rfid_scan);
    lv_xml_register_image(NULL, "icon_rfid_scan_lg", icon_rfid_scan_lg);
    lv_xml_register_image(NULL, "icon_signal", icon_signal);
    lv_xml_register_image(NULL, "icon_star", icon_star);
    lv_xml_register_image(NULL, "icon_wifi_high", icon_wifi_high);
    lv_xml_register_image(NULL, "icon_wifi_low", icon_wifi_low);
    lv_xml_register_image(NULL, "icon_wifi_zero", icon_wifi_zero);
    lv_xml_register_image(NULL, "logo_light_for_dark", logo_light_for_dark);
    lv_xml_register_image(NULL, "battery_empty", battery_empty);
    lv_xml_register_image(NULL, "battery_medium", battery_medium);
    lv_xml_register_image(NULL, "battery_full", battery_full);
    lv_xml_register_image(NULL, "battery_charging", battery_charging);
#endif

#if defined(LV_USE_XML) && LV_USE_XML == 0
    /*--------------------
     *  Permanent screens
     *-------------------*/
    /* If XML is enabled it's assumed that the permanent screens are created
     * manually from XML using lv_xml_create() */
    /* To allow screens to reference each other, create them all before calling the sceen create functions */
    age = lv_obj_create(NULL);
    berhasil = lv_obj_create(NULL);
    gender = lv_obj_create(NULL);
    home = lv_obj_create(NULL);
    mengukur = lv_obj_create(NULL);
    monitor = lv_obj_create(NULL);
    result = lv_obj_create(NULL);
    scanning = lv_obj_create(NULL);

    age_create();
    berhasil_create();
    gender_create();
    home_create();
    mengukur_create();
    monitor_create();
    result_create();
    scanning_create();
#endif
}

void ui_set_target(uint32_t target)
{
    ui_target = target;
}

uint32_t ui_get_target(void)
{
    return ui_target;
}

bool ui_check_target(uint32_t target)
{
    return (ui_target & target) ? true : false;
}

/* Callbacks */

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void check_font(lv_font_t ** font, const char * name)
{
    if (!(*font)) {
        *font = (lv_font_t *)LV_FONT_DEFAULT;
        LV_LOG_WARN("font `%s` was not set. Using `LV_FONT_DEFAULT` instead", name);
    }
}