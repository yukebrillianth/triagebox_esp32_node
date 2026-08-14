#ifndef TB_DEBUG_H
#define TB_DEBUG_H

/*
 * Serial console for demoing the triage flow without the STM32 attached, plus a
 * `stats` command for CPU/heap/stack numbers.
 *
 * Compiled in only when CONFIG_TB_DEBUG_CONSOLE is set (menuconfig ->
 * "TriageBox debug"). Off by default: it can fake patient vitals over USB.
 */
void tb_debug_console_start(void);

#endif /* TB_DEBUG_H */
