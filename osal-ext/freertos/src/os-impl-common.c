/*
 * os-impl-common.c — m2s-cfs OSAL FreeRTOS layer: init dispatch, idle loop, helpers.
 */
#include "os-freertos.h"
#include "os-shared-common.h"
#include "os-shared-idmap.h"

/* OS_IdleLoop_Impl() blocks here until OS_ApplicationShutdown_Impl() gives it. */
static StaticSemaphore_t OS_FreeRTOS_IdleSemBuf;
static SemaphoreHandle_t OS_FreeRTOS_IdleSem;

int32 OS_API_Impl_Init(osal_objtype_t idtype)
{
    int32 return_code;

    return_code = OS_FreeRTOS_TableMutex_Init(idtype);
    if (return_code != OS_SUCCESS)
    {
        return return_code;
    }

    switch (idtype)
    {
        case OS_OBJECT_TYPE_OS_TASK:
            OS_FreeRTOS_IdleSem = xSemaphoreCreateBinaryStatic(&OS_FreeRTOS_IdleSemBuf);
            return_code         = OS_FreeRTOS_TaskAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_QUEUE:
            return_code = OS_FreeRTOS_QueueAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_BINSEM:
            return_code = OS_FreeRTOS_BinSemAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_COUNTSEM:
            return_code = OS_FreeRTOS_CountSemAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_MUTEX:
            return_code = OS_FreeRTOS_MutexAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_TIMEBASE:
            return_code = OS_FreeRTOS_TimeBaseAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_FILESYS:
            return_code = OS_FreeRTOS_FileSysAPI_Impl_Init();
            break;
#ifdef OS_FREERTOS_HOST_SOCKETS
        case OS_OBJECT_TYPE_OS_STREAM:
            return_code = OS_FreeRTOS_HostSocketsInit();
            break;
#endif
        default:
            break;
    }

    return return_code;
}

void OS_IdleLoop_Impl(void)
{
    (void)xSemaphoreTake(OS_FreeRTOS_IdleSem, portMAX_DELAY);
}

void OS_ApplicationShutdown_Impl(void)
{
    (void)xSemaphoreGive(OS_FreeRTOS_IdleSem);
}

UBaseType_t OS_FreeRTOS_MapPriority(osal_priority_t priority)
{
    /* Levels 1 .. configMAX_PRIORITIES-2: 0 is the idle task, configMAX_PRIORITIES-1 the timer task. */
    const uint32 levels = (uint32)configMAX_PRIORITIES - 2U;

    return (UBaseType_t)(1U + (((uint32)OS_MAX_TASK_PRIORITY - (uint32)priority) * levels) /
                                  ((uint32)OS_MAX_TASK_PRIORITY + 1U));
}

TickType_t OS_FreeRTOS_MsToTicks(uint32 msecs)
{
    uint64_t ticks;

    if (msecs == 0)
    {
        return 0;
    }

    ticks = (((uint64_t)msecs * (uint64_t)configTICK_RATE_HZ) + 999U) / 1000U + 1U;
    if (ticks >= (uint64_t)portMAX_DELAY)
    {
        ticks = (uint64_t)portMAX_DELAY - 1U;
    }

    return (TickType_t)ticks;
}
