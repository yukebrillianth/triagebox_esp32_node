#include "ui_settings.h"

#include <stddef.h>

#include "ui_runtime.h" /* UI_RETRIAGE_DEFAULT_MS */

#define KEY_NODE_ID "node_id"
#define KEY_RETRIAGE "retriage"

static const ui_settings_store_t *s_store;
static uint8_t s_node_id; /* 0 = unset */
static uint32_t s_retriage_ms = UI_RETRIAGE_DEFAULT_MS;

static bool node_id_valid(uint32_t id)
{
    return (id >= 1U) && (id <= UI_NODE_ID_MAX);
}

static bool retriage_valid(uint32_t ms)
{
    return (ms == 0U) || (ms == 15000U) || (ms == 30000U) || (ms == 60000U);
}

void ui_settings_init(const ui_settings_store_t *store)
{
    uint32_t v;

    s_store = store;
    s_node_id = 0U;
    s_retriage_ms = UI_RETRIAGE_DEFAULT_MS;
    if (store == NULL) {
        return;
    }
    if (store->load(KEY_NODE_ID, &v) && node_id_valid(v)) {
        s_node_id = (uint8_t)v;
    }
    if (store->load(KEY_RETRIAGE, &v) && retriage_valid(v)) {
        s_retriage_ms = v;
    }
}

uint8_t ui_settings_node_id(void)
{
    return s_node_id;
}

bool ui_settings_set_node_id(uint8_t id)
{
    if (!node_id_valid(id)) {
        return false;
    }
    s_node_id = id; /* live now, even if the save below fails */
    return (s_store == NULL) || s_store->save(KEY_NODE_ID, id);
}

uint32_t ui_settings_retriage_ms(void)
{
    return s_retriage_ms;
}

bool ui_settings_set_retriage_ms(uint32_t ms)
{
    if (!retriage_valid(ms)) {
        return false;
    }
    s_retriage_ms = ms;
    return (s_store == NULL) || s_store->save(KEY_RETRIAGE, ms);
}
