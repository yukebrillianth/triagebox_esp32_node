#ifndef TB_UI_SOURCE_H
#define TB_UI_SOURCE_H

#include "ui_types.h"

/*
 * Device-side implementation of ui/logic/ui_mock.h: same header, but the data
 * comes from RS485 instead of a deterministic fake, and ui_mock_get_priority()
 * runs the SVM. The choice between the two is made in CMake (main/ links this
 * file, sim/ links ui_mock.c) — no #ifdef in ui/logic/.
 *
 * These four entry points are called from the tb_link RX task; everything in
 * ui_mock.h is called from the LVGL task. A spinlock guards the handoff.
 */
void tb_ui_source_on_vital(const vitals_t *v);
void tb_ui_source_on_button(uint8_t index, bool pressed);
void tb_ui_source_on_rfid(const rfid_t *r);
/* lora_ok < 0 means the STM32 did not include the (optional) 3rd payload byte. */
void tb_ui_source_on_status(uint8_t sensor_ok_mask, uint8_t battery, int lora_ok);

/* Latest sensor-OK bitmask from TB_FRAME_STATUS (0 until the first frame). */
uint8_t tb_ui_source_sensor_mask(void);

/* Called by the RX task on every accepted frame so the "Sistem" dot can tell
 * a live STM32 from a silent one. */
void tb_ui_source_mark_frame(void);

#endif /* TB_UI_SOURCE_H */
