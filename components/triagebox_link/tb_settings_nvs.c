#include "tb_settings.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "ui_settings.h"

static const char *TAG = "tb_settings";
static const char *NS = "triagebox";

static bool nvs_load(const char *key, uint32_t *out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);

    if (err != ESP_OK) {
        return false; /* first boot: the namespace does not exist yet */
    }
    err = nvs_get_u32(h, key, out);
    nvs_close(h);
    return err == ESP_OK;
}

static bool nvs_save(const char *key, uint32_t value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);

    if (err == ESP_OK) {
        err = nvs_set_u32(h, key, value);
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "could not save %s: %s", key, esp_err_to_name(err));
    }
    return err == ESP_OK;
}

static const ui_settings_store_t k_store = {.load = nvs_load, .save = nvs_save};

esp_err_t tb_settings_start(void)
{
    esp_err_t err = nvs_flash_init();

    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        /* Only for an unusable partition (first flash over an older layout):
         * erasing loses the two settings, which fall back to their defaults. */
        ESP_LOGW(TAG, "NVS unusable (%s), erasing", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (%s): settings are RAM-only this boot",
                 esp_err_to_name(err));
        ui_settings_init(NULL);
        return err;
    }
    ui_settings_init(&k_store);
    ESP_LOGI(TAG, "node id %u, re-triage %u ms",
             (unsigned)ui_settings_node_id(), (unsigned)ui_settings_retriage_ms());
    return ESP_OK;
}
