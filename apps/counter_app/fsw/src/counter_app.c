/*
 * counter_app.c — m2s-cfs counter_app.
 *
 * A minimal cFE application: registers with cFE, prints "counter_app initialized", then prints an
 * incrementing counter once per second until cFE asks it to stop. Output goes through OS_printf
 * (OSAL console -> BSP -> terminal or UART), so the same source runs on every target (D28).
 * Uses only the cFE and OSAL public APIs: no C library I/O, no platform code, no dynamic memory.
 *
 * Period: the loop waits until the next fixed 1 s deadline (OS_GetLocalTime) rather than a plain
 * OS_TaskDelay(1000). OS_TaskDelay waits at least the time asked, up to one tick more, so plain delays
 * drift (about 1 ms per second measured on the simulator); deadlines keep the error within about a tick.
 */
#include "cfe.h"

#define COUNTER_APP_PERIOD_MS 1000

/* Sleeps until *deadline + one period, then advances *deadline. Resynchronises if local time jumps. */
static void COUNTER_APP_WaitNextPeriod(OS_time_t *deadline)
{
    OS_time_t now;
    int64     remaining_ms;

    *deadline = OS_TimeAdd(*deadline, OS_TimeFromTotalMilliseconds(COUNTER_APP_PERIOD_MS));
    (void)OS_GetLocalTime(&now);
    remaining_ms = OS_TimeGetTotalMilliseconds(OS_TimeSubtract(*deadline, now));

    if (remaining_ms < 0 || remaining_ms > COUNTER_APP_PERIOD_MS)
    {
        /* Late by more than a period, or local time was set: start a new schedule from now. */
        *deadline    = OS_TimeAdd(now, OS_TimeFromTotalMilliseconds(COUNTER_APP_PERIOD_MS));
        remaining_ms = COUNTER_APP_PERIOD_MS;
    }
    if (remaining_ms > 0)
    {
        (void)OS_TaskDelay((uint32)remaining_ms);
    }
}

void COUNTER_APP_Main(void)
{
    uint32       run_status = CFE_ES_RunStatus_APP_RUN;
    uint32       counter    = 0;
    CFE_Status_t status;
    OS_time_t    deadline;

    /* Register with Event Services (no event filters). */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        OS_printf("counter_app: CFE_EVS_Register failed, status 0x%08lX\n", (unsigned long)status);
        run_status = CFE_ES_RunStatus_APP_ERROR;
    }
    else
    {
        OS_printf("counter_app initialized\n");
    }

    (void)OS_GetLocalTime(&deadline);

    while (CFE_ES_RunLoop(&run_status))
    {
        COUNTER_APP_WaitNextPeriod(&deadline);
        ++counter;
        OS_printf("counter_app counter: %lu\n", (unsigned long)counter);
    }

    CFE_ES_ExitApp(run_status);
}
