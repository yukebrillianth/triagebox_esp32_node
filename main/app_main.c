#include "ui_runtime.h"

void app_main(void)
{
    /* Link-check only: no board/panel bring-up, no ui_init / esp_lvgl_port yet. */
    ui_runtime_init();
}
