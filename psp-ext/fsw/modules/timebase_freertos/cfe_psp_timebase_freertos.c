/*
 * cfe_psp_timebase_freertos.c — m2s-cfs PSP module: cFE timebase from the FreeRTOS tick.
 *
 * Monotonic time since the scheduler started, with the tick's resolution (1 ms at 1 kHz).
 * The 32-bit tick count is extended to 64 bits here; this needs one call per tick wrap
 * (49.7 days at 1 kHz), which cFE TIME far exceeds. Not callable from interrupts.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "cfe_psp.h"
#include "cfe_psp_module.h"

CFE_PSP_MODULE_DECLARE_SIMPLE(timebase_freertos);

static TickType_t CFE_PSP_FreeRTOS_LastTick;
static uint64     CFE_PSP_FreeRTOS_TickHigh;

static uint64 CFE_PSP_FreeRTOS_Ticks64(void)
{
    TickType_t now;
    uint64     ticks;

    taskENTER_CRITICAL();
    now = xTaskGetTickCount();
    if (sizeof(TickType_t) < sizeof(uint64) && now < CFE_PSP_FreeRTOS_LastTick)
    {
        CFE_PSP_FreeRTOS_TickHigh += (uint64)((TickType_t)~(TickType_t)0) + 1U; /* one full wrap */
    }
    CFE_PSP_FreeRTOS_LastTick = now;
    ticks                     = CFE_PSP_FreeRTOS_TickHigh + (uint64)now;
    taskEXIT_CRITICAL();
    return ticks;
}

void timebase_freertos_Init(uint32 PspModuleId)
{
    (void)PspModuleId;
    OS_printf("CFE_PSP: using the FreeRTOS tick (%u Hz) as CFE timebase\n", (unsigned int)configTICK_RATE_HZ);
}

/* Timebase = 64-bit tick count, upper and lower halves. */
void CFE_PSP_Get_Timebase(uint32 *Tbu, uint32 *Tbl)
{
    uint64 ticks = CFE_PSP_FreeRTOS_Ticks64();

    *Tbu = (uint32)(ticks >> 32);
    *Tbl = (uint32)(ticks & 0xFFFFFFFFU);
}

void CFE_PSP_GetTime(OS_time_t *LocalTime)
{
    uint64 ticks = CFE_PSP_FreeRTOS_Ticks64();

    *LocalTime = OS_TimeFromTotalMicroseconds((int64)(ticks * (1000000ULL / configTICK_RATE_HZ)));
}

uint32 CFE_PSP_GetTimerTicksPerSecond(void)
{
    return configTICK_RATE_HZ;
}

/* The lower 32 bits wrap at 2^32 ticks (CFE_PSP_Get_Timebase), so there is no smaller rollover. */
uint32 CFE_PSP_GetTimerLow32Rollover(void)
{
    return 0;
}
