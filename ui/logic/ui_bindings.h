#ifndef UI_LOGIC_UI_BINDINGS_H
#define UI_LOGIC_UI_BINDINGS_H

void ui_bindings_init(void);

void ui_bindings_sync_selection(void);

/* Repaint the three Home status dots from ui_mock_get_link_status(). */
void ui_bindings_sync_status_dots(void);

void ui_bindings_start_scan_animation(void);

#endif
