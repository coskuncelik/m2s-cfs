/*
 * bsp_console.c — m2s-cfs OSAL BSP "freertos-posix-sim": console output to the host stdout.
 *
 * Uses write(2), not stdio: stdio takes glibc locks, which can deadlock when the FreeRTOS
 * POSIX port switches tasks while one holds such a lock. The tick signal can interrupt
 * write(2), so partial writes and EINTR are retried.
 */
#include <errno.h>
#include <unistd.h>

#include "osapi.h"
#include "bsp-impl.h"

void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    ssize_t written;

    while (DataLen > 0)
    {
        written = write(STDOUT_FILENO, Str, DataLen);
        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        Str += written;
        DataLen -= (size_t)written;
    }
}

void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
    (void)ModeBits; /* no terminal colour control in the simulator */
}
