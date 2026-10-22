#ifndef UI_LOGIC_UI_SETTINGS_H
#define UI_LOGIC_UI_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The few settings that must survive a reboot: this node's LoRa poll address and
 * the Monitor re-triage interval.
 *
 * Demo mode is deliberately NOT here -- see ui_demo.h, where "off at every boot"
 * is a safety property. These two are the opposite: a board that comes back from
 * a brown-out on a different address, or silently on the default interval, is the
 * failure this module exists to remove.
 *
 * Platform-neutral on purpose. Storage is a pair of callbacks: the device passes
 * an NVS-backed store (components/triagebox_link/tb_settings_nvs.c), the desktop
 * sim and the host tests pass NULL and get plain RAM. A read that fails or holds
 * an out-of-range value yields the default -- a corrupt page must not be able to
 * put the node on address 255 or switch re-triage off.
 */

/* Highest poll address the station will ever call (LORA_POLL_NODE_MAX there). */
#define UI_NODE_ID_MAX 20U

typedef struct {
    bool (*load)(const char *key, uint32_t *out); /* false = absent/unreadable */
    bool (*save)(const char *key, uint32_t value); /* false = not persisted */
} ui_settings_store_t;

/* Reads both values from @p store (may be NULL = RAM only). Call once, before
 * ui_runtime_init(). Safe to call again; it re-reads. */
void ui_settings_init(const ui_settings_store_t *store);

/* 1..UI_NODE_ID_MAX, or 0 when nobody has chosen one. 0 is not a default address:
 * an unset board stays silent on the radio rather than claiming somebody's id. */
uint8_t ui_settings_node_id(void);

/* False when @p id is outside 1..UI_NODE_ID_MAX (nothing changes) or when it was
 * applied but could not be saved -- the caller should say so, because it will be
 * gone after a reboot. */
bool ui_settings_set_node_id(uint8_t id);

/* One of 0 (off), 15000, 30000, 60000. Same false-return rule as the node id. */
uint32_t ui_settings_retriage_ms(void);
bool ui_settings_set_retriage_ms(uint32_t ms);

#endif /* UI_LOGIC_UI_SETTINGS_H */
