#include "ui_mock.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    ui_mock_init();

    /* RFID after 500ms */
    ui_mock_start_scan();
    rfid_t tag;
    assert(!ui_mock_rfid_ready(&tag));
    ui_mock_tick(100);
    assert(!ui_mock_rfid_ready(&tag));
    ui_mock_tick(500);
    assert(ui_mock_rfid_ready(&tag));
    assert(tag.present);
    assert(strcmp(tag.tag, "3021") == 0);
    assert(!ui_mock_rfid_ready(&tag)); /* one-shot */
    printf("rfid_ok tag=%s\n", "3021");

    /* Measure progress over UI_MEASURE_MS */
    ui_mock_init();
    ui_mock_tick(0);
    ui_mock_start_measure();
    assert(ui_mock_measure_progress() == 0);
    assert(!ui_mock_measure_done());
    ui_mock_tick(UI_MEASURE_MS / 2);
    {
        uint8_t p = ui_mock_measure_progress();
        printf("progress_mid=%u\n", (unsigned)p);
        assert(p >= 40 && p <= 60);
        assert(!ui_mock_measure_done());
    }
    ui_mock_tick(UI_MEASURE_MS);
    assert(ui_mock_measure_progress() == 100);
    assert(ui_mock_measure_done());
    printf("measure_done progress=100 measure_ms=%u\n", (unsigned)UI_MEASURE_MS);

    /* Priority cycle GREEN→YELLOW→RED→BLACK */
    ui_mock_init();
    {
        ui_priority_t expected[] = {
            UI_PRIORITY_GREEN, UI_PRIORITY_YELLOW,
            UI_PRIORITY_RED, UI_PRIORITY_BLACK, UI_PRIORITY_GREEN
        };
        int i;
        for (i = 0; i < 5; i++) {
            ui_priority_t p = ui_mock_get_priority();
            printf("priority[%d]=%d conf=%.2f reasons=\"%s\"\n",
                   i, (int)p, (double)ui_mock_get_confidence(),
                   ui_mock_get_reasons());
            assert(p == expected[i]);
            ui_mock_cycle_priority();
        }
    }

    /* Vitals base + valid */
    {
        vitals_t v;
        ui_mock_get_vitals(&v);
        assert(v.valid);
        assert(v.hr >= 87 && v.hr <= 93);
        assert(v.spo2 >= 97 && v.spo2 <= 99);
        assert(v.battery == 80);
        printf("vitals hr=%u spo2=%u rr=%u bp=%u/%u bat=%u valid=%d\n",
               (unsigned)v.hr, (unsigned)v.spo2, (unsigned)v.rr,
               (unsigned)v.bp_sys, (unsigned)v.bp_dia,
               (unsigned)v.battery, (int)v.valid);
    }

    /* Button single-slot */
    {
        btn_event_t e;
        assert(!ui_mock_pop_button(&e));
        ui_mock_tick(42);
        ui_mock_push_button(2, true);
        assert(ui_mock_pop_button(&e));
        assert(e.index == 2 && e.pressed && e.timestamp_ms == 42);
        assert(!ui_mock_pop_button(&e));
        printf("button_ok index=2 ts=42\n");
    }

    printf("ALL_PASS\n");
    return 0;
}
