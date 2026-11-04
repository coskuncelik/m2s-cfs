/*
 * freertos_posix_hooks.c — m2s-cfs, Linux simulator only (FreeRTOS GCC/Posix port).
 *
 * Idle hook: the stock idle task busy-spins on a host thread. On the development VM a
 * spinning thread can starve the port's tick thread for seconds (measured: one 1 ms sleep
 * took 14.9 s with two spinners on 2 vCPUs), which freezes the scheduler. Sleeping here
 * gives the CPU back; the tick (SIGALRM) interrupts the sleep. See docs/DECISIONS.md D15.
 */
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

void vApplicationIdleHook( void )
{
    const struct timespec xSleep = { 0, 1000000L }; /* 1 ms */

    ( void ) nanosleep( &xSleep, NULL );
}
