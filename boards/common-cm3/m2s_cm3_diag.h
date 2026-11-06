/*
 * m2s_cm3_diag.h — m2s-cfs generic Cortex-M3: last-words diagnostics and halt.
 *
 * Used by the trap, assert, panic and fault paths. Writes straight to the board console
 * (M2S_Board_ConsoleWrite, polled), never through stdio or OSAL, so it works with interrupts off, from an
 * exception handler and without a heap. Every message is one line starting with "M2S ", which the test
 * scripts look for:
 *   M2S TRAP   — dynamic allocation attempted (malloc family, _sbrk) or a FreeRTOS stack overflow
 *   M2S ASSERT — configASSERT failed
 *   M2S FAULT  — CPU fault exception (registers follow)
 *   M2S PANIC  — CFE_PSP_Panic
 *   M2S STOP   — the BSP stopped (application ended, scheduler ended, shutdown)
 */
#ifndef M2S_CM3_DIAG_H
#define M2S_CM3_DIAG_H

#include <stdint.h>

/* Writes a NUL-terminated string to the board console. */
void M2S_Diag_Write(const char *Str);

/* Writes "0x" and 8 hex digits. */
void M2S_Diag_WriteHex(uint32_t Value);

/* Writes an unsigned decimal number. */
void M2S_Diag_WriteDec(uint32_t Value);

/* Disables interrupts and stops the CPU here (a debugger can still attach). */
void M2S_Halt(void) __attribute__((noreturn));

#endif /* M2S_CM3_DIAG_H */
