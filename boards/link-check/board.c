/*
 * board.c — m2s-cfs board "link-check": PLACEHOLDER board services (see m2s_board.h).
 */
#include "m2s_board.h"

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
