/*
 * m2s_board.h — m2s-cfs board "link-check": PLACEHOLDER board for the step 08 cross-build.
 *
 * This board exists only so the Cortex-M3 image can be compiled and linked before the real board
 * support exists (no Microchip sources yet, D7). Nothing here describes real hardware:
 *  - the clock and NVIC priority bits are placeholders (the SmartFusion2 values come with the board step);
 *  - the console discards output;
 *  - the memory map (board.ld) is large enough for any footprint, so the link reports sizes instead
 *    of failing on them.
 * Interface every board provides: this header, M2S_Board_Init(), M2S_Board_ConsoleWrite() and a linker
 * script (board.ld); the generic start-up and newlib stubs come from boards/common-cm3.
 */
#ifndef M2S_BOARD_H
#define M2S_BOARD_H

/* PLACEHOLDER: CPU clock feeding SysTick, in Hz. */
#define M2S_BOARD_CPU_CLOCK_HZ 100000000UL

/* PLACEHOLDER: number of implemented NVIC priority bits (ARMv7-M allows 3 to 8). */
#define M2S_BOARD_NVIC_PRIO_BITS 4

#ifndef __ASSEMBLER__
#include <stddef.h>

/* Called first in main(), before any FreeRTOS or OSAL call: clocks, console device. */
void M2S_Board_Init(void);

/* Writes DataLen bytes to the board console (blocking). Used by the OSAL BSP console output. */
void M2S_Board_ConsoleWrite(const char *Str, size_t DataLen);
#endif

#endif /* M2S_BOARD_H */
