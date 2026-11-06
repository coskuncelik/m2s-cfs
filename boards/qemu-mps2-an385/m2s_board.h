/*
 * m2s_board.h — m2s-cfs board "qemu-mps2-an385": QEMU's ARM MPS2 FPGA board, image AN385 (Cortex-M3), step 09.
 *
 * Emulated, not the SmartFusion2. Values checked on QEMU 8.2.2 (`info mtree`, `info qtree`, 2026-11-06):
 * CPU clock 25 MHz (SysTick "cpuclk"); SSRAM1 4 MiB at 0x00000000 (code, vector table); PSRAM 16 MiB at
 * 0x21000000 (data, bss, main stack); UART0 = CMSDK APB UART at 0x40004000 (pclk 25 MHz).
 * NVIC priority bits: 3, as on the real MPS2 Cortex-M3 (QEMU emulates at least that many; the FreeRTOS
 * port checks the priority settings against the hardware at scheduler start).
 * Board interface: this header, M2S_Board_Init(), M2S_Board_ConsoleWrite(), board.ld; the generic
 * start-up and newlib stubs come from boards/common-cm3.
 */
#ifndef M2S_BOARD_H
#define M2S_BOARD_H

/* CPU clock feeding SysTick, in Hz. */
#define M2S_BOARD_CPU_CLOCK_HZ 25000000UL

/* Number of implemented NVIC priority bits. */
#define M2S_BOARD_NVIC_PRIO_BITS 3

#ifndef __ASSEMBLER__
#include <stddef.h>

/* Called first in main(), before any FreeRTOS or OSAL call: console UART. */
void M2S_Board_Init(void);

/* Writes DataLen bytes to UART0 (polled, blocking; usable with interrupts off). */
void M2S_Board_ConsoleWrite(const char *Str, size_t DataLen);
#endif

#endif /* M2S_BOARD_H */
