/*
 * board.c — m2s-cfs board "qemu-mps2-an385": console on UART0 (ARM CMSDK APB UART), polled.
 *
 * Registers (ARM CMSDK, DDI 0479): DATA +0x00, STATE +0x04 (bit 0: TX buffer full), CTRL +0x08 (bit 0: TX
 * enable), BAUDDIV +0x10 (at least 16). QEMU drops writes while TX is disabled. With -nographic or
 * -serial stdio the output appears on the host terminal.
 *
 * Exit: ARM semihosting SYS_EXIT_EXTENDED (operation 0x20, "BKPT 0xAB" on M-profile), so QEMU ends with the
 * program's status. Only on this emulated board: on real hardware without a debugger the BKPT would fault.
 * QEMU must run with -semihosting-config enable=on,target=native; otherwise the BKPT ends in the fault
 * handler, so the exit status is printed first.
 */
#include <stdint.h>

#include "m2s_board.h"
#include "m2s_cm3_diag.h"

#define M2S_UART0_BASE    0x40004000UL
#define M2S_UART_DATA     (*(volatile uint32_t *)(M2S_UART0_BASE + 0x00UL))
#define M2S_UART_STATE    (*(volatile uint32_t *)(M2S_UART0_BASE + 0x04UL))
#define M2S_UART_CTRL     (*(volatile uint32_t *)(M2S_UART0_BASE + 0x08UL))
#define M2S_UART_BAUDDIV  (*(volatile uint32_t *)(M2S_UART0_BASE + 0x10UL))
#define M2S_UART_TXFULL   (1UL << 0)
#define M2S_UART_TXEN     (1UL << 0)
#define M2S_UART_BAUDRATE 115200UL

void M2S_Board_Init(void)
{
    M2S_UART_BAUDDIV = M2S_BOARD_CPU_CLOCK_HZ / M2S_UART_BAUDRATE;
    M2S_UART_CTRL    = M2S_UART_TXEN;
}

void M2S_Board_ConsoleWrite(const char *Str, size_t DataLen)
{
    size_t i;

    if ((M2S_UART_CTRL & M2S_UART_TXEN) == 0U)
    {
        M2S_Board_Init(); /* a fault before M2S_Board_Init() can still report */
    }
    for (i = 0; i < DataLen; ++i)
    {
        if (Str[i] == '\n')
        {
            while ((M2S_UART_STATE & M2S_UART_TXFULL) != 0U)
            {
            }
            M2S_UART_DATA = (uint32_t)'\r';
        }
        while ((M2S_UART_STATE & M2S_UART_TXFULL) != 0U)
        {
        }
        M2S_UART_DATA = (uint32_t)(unsigned char)Str[i];
    }
}

#define M2S_SEMIHOST_SYS_EXIT_EXTENDED    0x20UL
#define M2S_SEMIHOST_ADP_APPLICATION_EXIT 0x20026UL

void M2S_Board_Exit(int Status)
{
    uint32_t block[2];

    M2S_Diag_Write("\nM2S EXIT status ");
    M2S_Diag_WriteDec((uint32_t)Status);
    M2S_Diag_Write("\n");

    block[0] = M2S_SEMIHOST_ADP_APPLICATION_EXIT;
    block[1] = (uint32_t)Status;
    __asm volatile("mov r0, %0\n"
                   "mov r1, %1\n"
                   "bkpt 0xab\n"
                   :
                   : "r"(M2S_SEMIHOST_SYS_EXIT_EXTENDED), "r"(block)
                   : "r0", "r1", "memory");
    M2S_Halt(); /* only if semihosting is off and the fault handler returned (it does not) */
}
