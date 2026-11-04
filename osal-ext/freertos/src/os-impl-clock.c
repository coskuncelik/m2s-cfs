/*
 * os-impl-clock.c — m2s-cfs OSAL FreeRTOS layer: local time from the FreeRTOS tick.
 *
 * Local time = time set by OS_SetLocalTime() + ticks elapsed since then. The 32-bit tick
 * count is extended to 64 bits here; this requires a call at least once per tick wrap
 * (49.7 days at 1 kHz), which cFE time services far exceed.
 * Before any OS_SetLocalTime() call, local time is the time since boot.
 */
#include "os-freertos.h"
#include "os-shared-clock.h"

static TickType_t OS_FreeRTOS_LastTick;
static uint64_t   OS_FreeRTOS_TickHigh;
static uint64_t   OS_FreeRTOS_EpochTicks;
static OS_time_t  OS_FreeRTOS_EpochTime;

/* 64-bit tick count. Caller must be in a critical section. */
static uint64_t OS_FreeRTOS_Ticks64(void)
{
    TickType_t now = xTaskGetTickCount();

    if (sizeof(TickType_t) < sizeof(uint64_t) && now < OS_FreeRTOS_LastTick)
    {
        OS_FreeRTOS_TickHigh += (uint64_t)((TickType_t)~(TickType_t)0) + 1U; /* one full wrap */
    }
    OS_FreeRTOS_LastTick = now;
    return OS_FreeRTOS_TickHigh + (uint64_t)now;
}

int32 OS_GetLocalTime_Impl(OS_time_t *time_struct)
{
    uint64_t  elapsed;
    OS_time_t epoch;

    taskENTER_CRITICAL();
    elapsed = OS_FreeRTOS_Ticks64() - OS_FreeRTOS_EpochTicks;
    epoch   = OS_FreeRTOS_EpochTime;
    taskEXIT_CRITICAL();

    *time_struct = OS_TimeAdd(epoch, OS_TimeFromTotalMicroseconds((int64)(elapsed * (1000000ULL / configTICK_RATE_HZ))));
    return OS_SUCCESS;
}

int32 OS_SetLocalTime_Impl(const OS_time_t *time_struct)
{
    taskENTER_CRITICAL();
    OS_FreeRTOS_EpochTicks = OS_FreeRTOS_Ticks64();
    OS_FreeRTOS_EpochTime  = *time_struct;
    taskEXIT_CRITICAL();
    return OS_SUCCESS;
}
