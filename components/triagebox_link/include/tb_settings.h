#ifndef TB_SETTINGS_H
#define TB_SETTINGS_H

#include "esp_err.h"

/*
 * Brings up NVS and hands ui_settings an NVS-backed store. Call once from
 * app_main(), BEFORE ui_runtime_init(), so the Menu and the Monitor timer start
 * from the saved values.
 *
 * Never fatal: if flash cannot be initialised the settings run from RAM with
 * their defaults and this returns the error for the log. A node that cannot
 * remember its address is still a node.
 */
esp_err_t tb_settings_start(void);

#endif /* TB_SETTINGS_H */
