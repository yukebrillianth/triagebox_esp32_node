/*
 * ui_settings: validation, persistence round trip, and what a bad store does.
 * The store is a fake with a flip-able failure, so no flash is involved.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui_mock.h"
#include "ui_runtime.h"
#include "ui_settings.h"

static struct {
    bool has_node, has_retriage, has_measure;
    uint32_t node, retriage, measure;
    bool fail_save;
    int saves;
} s_fake;

static bool fake_load(const char *key, uint32_t *out)
{
    if (strcmp(key, "node_id") == 0 && s_fake.has_node) {
        *out = s_fake.node;
        return true;
    }
    if (strcmp(key, "retriage") == 0 && s_fake.has_retriage) {
        *out = s_fake.retriage;
        return true;
    }
    if (strcmp(key, "measure") == 0 && s_fake.has_measure) {
        *out = s_fake.measure;
        return true;
    }
    return false;
}

static bool fake_save(const char *key, uint32_t v)
{
    if (s_fake.fail_save) {
        return false;
    }
    s_fake.saves++;
    if (strcmp(key, "node_id") == 0) {
        s_fake.has_node = true;
        s_fake.node = v;
    } else if (strcmp(key, "measure") == 0) {
        s_fake.has_measure = true;
        s_fake.measure = v;
    } else {
        s_fake.has_retriage = true;
        s_fake.retriage = v;
    }
    return true;
}

static const ui_settings_store_t k_store = {fake_load, fake_save};

int main(void)
{
    /* Nothing saved: an unset node id (silent on the radio) and the default. */
    memset(&s_fake, 0, sizeof(s_fake));
    ui_settings_init(&k_store);
    assert(ui_settings_node_id() == 0U);
    assert(ui_settings_retriage_ms() == UI_RETRIAGE_DEFAULT_MS);

    /* The node id is 1..UI_NODE_ID_MAX; anything else changes nothing. */
    assert(!ui_settings_set_node_id(0U));
    assert(!ui_settings_set_node_id(UI_NODE_ID_MAX + 1U));
    assert(!ui_settings_set_node_id(255U));
    assert(ui_settings_node_id() == 0U && s_fake.saves == 0);
    assert(ui_settings_set_node_id(1U) && ui_settings_set_node_id(UI_NODE_ID_MAX));
    assert(ui_settings_node_id() == UI_NODE_ID_MAX);

    /* Survives a "reboot": a fresh init reads back what was saved. */
    assert(ui_settings_set_node_id(7U) && ui_settings_set_retriage_ms(60000U));
    ui_settings_init(&k_store);
    assert(ui_settings_node_id() == 7U);
    assert(ui_settings_retriage_ms() == 60000U);

    /* "Off" is a real choice and must round trip as 0, not fall to the default. */
    assert(ui_settings_set_retriage_ms(0U));
    ui_settings_init(&k_store);
    assert(ui_settings_retriage_ms() == 0U);

    /* Every non-zero choice is at least one station poll cycle, and the default
     * is one of the choices. */
    for (unsigned i = 0; i < UI_RETRIAGE_CHOICES; i++) {
        assert(ui_settings_retriage_choices[i] == 0U ||
               ui_settings_retriage_choices[i] >= UI_RETRIAGE_MIN_MS);
    }
    assert(UI_RETRIAGE_DEFAULT_MS >= UI_RETRIAGE_MIN_MS);
    assert(ui_settings_set_retriage_ms(UI_RETRIAGE_DEFAULT_MS));
    assert(ui_settings_set_retriage_ms(0U));

    /* Only the Menu's intervals are accepted. */
    assert(!ui_settings_set_retriage_ms(1234U));
    assert(ui_settings_retriage_ms() == 0U);

    /* Old builds saved 15 s / 30 s; those options are gone (a re-triage needs a
     * BP, which needs a 30-60 s recording), so they read back as the default. */
    s_fake.has_retriage = true;
    s_fake.retriage = 15000U;
    ui_settings_init(&k_store);
    assert(ui_settings_retriage_ms() == UI_RETRIAGE_DEFAULT_MS);
    s_fake.retriage = 30000U;
    ui_settings_init(&k_store);
    assert(ui_settings_retriage_ms() == UI_RETRIAGE_DEFAULT_MS);
    for (unsigned i = 0; i < UI_RETRIAGE_CHOICES; i++) {
        const uint32_t c = ui_settings_retriage_choices[i];
        assert(c == 0U || c == 60000U || c == 300000U || c == 900000U);
    }

    /* First-measurement duration: 30 s or 60 s only, default 60 s on hardware,
     * survives a reboot, and a corrupt value falls back. */
    assert(UI_MEASURE_MS_DEFAULT == 60000U);
    memset(&s_fake, 0, sizeof(s_fake));
    ui_settings_init(&k_store);
    assert(ui_settings_measure_ms() == UI_MEASURE_MS);
    assert(!ui_settings_set_measure_ms(0U) && !ui_settings_set_measure_ms(45000U) &&
           !ui_settings_set_measure_ms(2000U));
    assert(ui_settings_measure_ms() == UI_MEASURE_MS && s_fake.saves == 0);
    assert(ui_settings_set_measure_ms(30000U));
    ui_settings_init(&k_store);
    assert(ui_settings_measure_ms() == 30000U);
    assert(ui_settings_set_measure_ms(60000U));
    ui_settings_init(&k_store);
    assert(ui_settings_measure_ms() == 60000U);
    s_fake.measure = 45000U;
    s_fake.has_measure = true;
    ui_settings_init(&k_store);
    assert(ui_settings_measure_ms() == UI_MEASURE_MS);
    memset(&s_fake, 0, sizeof(s_fake));
    ui_settings_init(&k_store);

    /* A corrupt page cannot put the node on a bad address or change re-triage. */
    s_fake.has_node = s_fake.has_retriage = true;
    s_fake.node = 200U;
    s_fake.retriage = 5U;
    ui_settings_init(&k_store);
    assert(ui_settings_node_id() == 0U);
    assert(ui_settings_retriage_ms() == UI_RETRIAGE_DEFAULT_MS);
    s_fake.node = 0U;
    ui_settings_init(&k_store);
    assert(ui_settings_node_id() == 0U);

    /* A failing save still applies the value for this run but says so, so the
     * Menu can warn that it will not survive. */
    s_fake.fail_save = true;
    assert(!ui_settings_set_node_id(9U));
    assert(ui_settings_node_id() == 9U);
    assert(!ui_settings_set_retriage_ms(60000U));
    assert(ui_settings_retriage_ms() == 60000U);
    assert(!ui_settings_set_measure_ms(30000U));
    assert(ui_settings_measure_ms() == 30000U);

    /* No store (the sim): RAM only, and every valid set reports success. */
    ui_settings_init(NULL);
    assert(ui_settings_node_id() == 0U);
    assert(ui_settings_set_node_id(3U) && ui_settings_node_id() == 3U);
    assert(ui_settings_set_retriage_ms(900000U));
    assert(ui_settings_measure_ms() == UI_MEASURE_MS);

    printf("ui_settings_selftest: OK\n");
    return 0;
}
