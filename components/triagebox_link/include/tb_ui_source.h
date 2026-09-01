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

/*
 * Downlink RSSI in dBm, from TB_REG_LORA_RSSI: how strongly the node heard the
 * station's last poll. Validity is decided here with tb_rssi_valid() rather than
 * by the caller, so the two "no reading" bytes (0 from a fresh STM32, 0xFF from
 * an old one's pad) collapse to one answer in one place.
 */
void tb_ui_source_on_rssi(int8_t dbm);

/* Latest sensor-OK bitmask from TB_FRAME_STATUS (0 until the first frame). */
uint8_t tb_ui_source_sensor_mask(void);

/*
 * The BP result path. bp_capture.c (BP task) publishes through on_bp(); the
 * overlay is folded into every snapshot tb_ui_source_on_vital() copies, so
 * Result/Monitor render it and tb_triage_classify() consumes it with no extra
 * wiring. bp_arm() clears the overlay and the settled flag (measure-start);
 * bp_ready() is the flag infer_once() bounded-waits on (first publish after
 * arming sets it, valid or not).
 */
void tb_ui_source_on_bp(bool valid, uint16_t sys, uint16_t dia);
bool tb_ui_source_bp_ready(void);
void tb_ui_source_bp_arm(void);

/*
 * Button events the queue had to refuse. Non-zero means the UI stopped draining
 * (LVGL task blocked) -- not that the operator pressed too fast. Surfaced by the
 * `stats` console command.
 */
uint32_t tb_ui_source_buttons_dropped(void);

/* Called by the RX task on every accepted frame so the "Sistem" dot can tell
 * a live STM32 from a silent one. */
void tb_ui_source_mark_frame(void);

#endif /* TB_UI_SOURCE_H */
