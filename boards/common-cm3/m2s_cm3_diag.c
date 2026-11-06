/*
 * m2s_cm3_diag.c — m2s-cfs generic Cortex-M3: last-words diagnostics and halt (see m2s_cm3_diag.h).
 */
#include <stddef.h>

#include "m2s_board.h"
#include "m2s_cm3_diag.h"

void M2S_Diag_Write(const char *Str)
{
    size_t len = 0;

    while (Str[len] != '\0')
    {
        ++len;
    }
    M2S_Board_ConsoleWrite(Str, len);
}

void M2S_Diag_WriteHex(uint32_t Value)
{
    static const char digits[] = "0123456789abcdef";
    char              buf[10];
    int               i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i)
    {
        buf[2 + i] = digits[(Value >> (28 - 4 * i)) & 0xFU];
    }
    M2S_Board_ConsoleWrite(buf, sizeof(buf));
}

void M2S_Diag_WriteDec(uint32_t Value)
{
    char   buf[10];
    size_t pos = sizeof(buf);

    do
    {
        buf[--pos] = (char)('0' + (Value % 10U));
        Value /= 10U;
    } while (Value != 0U && pos > 0U);
    M2S_Board_ConsoleWrite(&buf[pos], sizeof(buf) - pos);
}

/* Defined only in test images (test_heap.c). */
extern void M2S_TestHeap_Report(void) __attribute__((weak));

void M2S_Program_Exit(int Status)
{
    __asm volatile("cpsid i" ::: "memory");
    if (M2S_TestHeap_Report != NULL)
    {
        M2S_TestHeap_Report();
    }
    M2S_Board_Exit(Status);
}

void M2S_Halt(void)
{
    __asm volatile("cpsid i" ::: "memory");
    for (;;)
    {
        /* stopped: see the last "M2S ..." console line, or attach a debugger */
    }
}
