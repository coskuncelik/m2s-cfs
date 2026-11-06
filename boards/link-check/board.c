/*
 * board.c — m2s-cfs board "link-check": PLACEHOLDER board services (see m2s_board.h).
 */
#include <stdint.h>

#include "m2s_board.h"
#include "m2s_cm3_diag.h"

/* Bytes the console would have written; lets a debugger see that output happened. */
volatile size_t M2S_Board_ConsoleBytes;

void M2S_Board_Init(void)
{
    /* Nothing to set up: no real hardware behind this board. */
}

void M2S_Board_ConsoleWrite(const char *Str, size_t DataLen)
{
    (void)Str;
    M2S_Board_ConsoleBytes += DataLen; /* output discarded */
}

void M2S_Board_Exit(int Status)
{
    M2S_Diag_Write("\nM2S STOP exit status ");
    M2S_Diag_WriteDec((uint32_t)Status);
    M2S_Diag_Write("\n");
    M2S_Halt();
}
