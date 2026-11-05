/*
 * bsp_console.c — m2s-cfs OSAL BSP "freertos-arm-cm3": console output to the board console
 * (M2S_Board_ConsoleWrite: a UART on a real board). Called from the OSAL console task.
 */
#include "osapi.h"
#include "bsp-impl.h"
#include "m2s_board.h"

void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    M2S_Board_ConsoleWrite(Str, DataLen);
}

void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
    (void)ModeBits; /* no terminal colour control */
}
