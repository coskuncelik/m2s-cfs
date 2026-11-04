/*
 * os-impl-timebase.c — m2s-cfs OSAL FreeRTOS layer: time bases.
 *
 * Each time base has a static helper task that runs OS_TimeBase_CallbackThread(). For an
 * internal time base the sync function sleeps until an absolute tick deadline (start time,
 * then every interval) and returns the elapsed microseconds. OS_TimeBaseSet_Impl() and
 * delete wake the helper through a static binary semaphore so it re-reads its settings.
 *
 * Delete is cooperative: the shared layer has marked the record reserved, so once the
 * helper wakes it leaves OS_TimeBase_CallbackThread(), signals "exited" and waits to be
 * deleted. If it does not stop in time (e.g. stuck in a user sync function), it is
 * deleted anyway. No dynamic memory and no FreeRTOS timer service.
 */
#include "os-freertos.h"
#include "os-shared-timebase.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

typedef struct
{
    StaticTask_t      tcb;
    StackType_t       stack[OS_FREERTOS_TIMEBASE_STACK_BYTES / sizeof(StackType_t)];
    TaskHandle_t      task;
    StaticSemaphore_t handler_mutex_buf;
    SemaphoreHandle_t handler_mutex;
    StaticSemaphore_t wake_buf;
    SemaphoreHandle_t wake;
    StaticSemaphore_t exited_buf;
    SemaphoreHandle_t exited;
    volatile bool     shutdown;
    /* Settings, read and written inside critical sections */
    bool       armed;
    bool       first;
    TickType_t next_wake;
    TickType_t interval_ticks;
    uint32     start_usec;
    uint32     interval_usec;
} OS_impl_timebase_internal_record_t;

static OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

static uint32 OS_FreeRTOS_UsecPerTick(void)
{
    return (uint32)(1000000UL / configTICK_RATE_HZ);
}

/* Microseconds -> ticks for a period: rounded up, at least one tick. */
static TickType_t OS_FreeRTOS_UsecToTicks(uint32 usecs)
{
    uint64_t ticks = ((uint64_t)usecs * configTICK_RATE_HZ + 999999U) / 1000000U;

    return (ticks == 0) ? 1 : (TickType_t)ticks;
}

static OS_impl_timebase_internal_record_t *OS_FreeRTOS_TimeBaseFromId(osal_id_t timebase_id)
{
    osal_index_t index;

    if (OS_ObjectIdToArrayIndex(OS_OBJECT_TYPE_OS_TIMEBASE, timebase_id, &index) != OS_SUCCESS)
    {
        return NULL;
    }
    return &OS_impl_timebase_table[index];
}

/* Internal sync function: block until the next deadline; return the elapsed time in usec. */
static uint32 OS_FreeRTOS_TimeBaseSync(osal_id_t timebase_id)
{
    OS_impl_timebase_internal_record_t *impl = OS_FreeRTOS_TimeBaseFromId(timebase_id);
    uint32                              tick_usec;
    bool                                armed;
    TickType_t                          next_wake;
    int32_t                             remaining;

    if (impl == NULL)
    {
        return 0;
    }

    for (;;)
    {
        if (impl->shutdown)
        {
            return 0;
        }

        taskENTER_CRITICAL();
        armed     = impl->armed;
        next_wake = impl->next_wake;
        taskEXIT_CRITICAL();

        if (!armed)
        {
            (void)xSemaphoreTake(impl->wake, portMAX_DELAY);
            continue;
        }

        remaining = (int32_t)(next_wake - xTaskGetTickCount());
        if (remaining > 0)
        {
            (void)xSemaphoreTake(impl->wake, (TickType_t)remaining); /* woken early = re-check */
            continue;
        }

        taskENTER_CRITICAL();
        if (impl->armed && impl->next_wake == next_wake)
        {
            tick_usec   = impl->first ? impl->start_usec : impl->interval_usec;
            impl->first = false;
            if (impl->interval_ticks > 0)
            {
                impl->next_wake += impl->interval_ticks;
            }
            else
            {
                impl->armed = false;
            }
        }
        else
        {
            tick_usec = 0; /* settings changed meanwhile */
        }
        taskEXIT_CRITICAL();

        if (tick_usec != 0)
        {
            return tick_usec;
        }
    }
}

static void OS_FreeRTOS_TimeBaseTask(void *arg)
{
    osal_id_t                           timebase_id = OS_ObjectIdFromInteger((unsigned long)(uintptr_t)arg);
    OS_impl_timebase_internal_record_t *impl        = OS_FreeRTOS_TimeBaseFromId(timebase_id);

    OS_TimeBase_CallbackThread(timebase_id);

    /* Record deleted: tell OS_TimeBaseDelete_Impl() and wait to be deleted. */
    if (impl != NULL)
    {
        (void)xSemaphoreGive(impl->exited);
    }
    for (;;)
    {
        vTaskSuspend(NULL);
    }
}

int32 OS_FreeRTOS_TimeBaseAPI_Impl_Init(void)
{
    memset(OS_impl_timebase_table, 0, sizeof(OS_impl_timebase_table));
    OS_SharedGlobalVars.TicksPerSecond  = (int32)configTICK_RATE_HZ;
    OS_SharedGlobalVars.MicroSecPerTick = (int32)OS_FreeRTOS_UsecPerTick();
    return OS_SUCCESS;
}

void OS_TimeBaseLock_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    (void)xSemaphoreTake(impl->handler_mutex, portMAX_DELAY);
}

void OS_TimeBaseUnlock_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    (void)xSemaphoreGive(impl->handler_mutex);
}

int32 OS_TimeBaseCreate_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl     = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    OS_timebase_internal_record_t *     timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);
    osal_id_t                           id       = OS_ObjectIdFromToken(token);

    memset(impl, 0, sizeof(*impl));
    impl->handler_mutex = xSemaphoreCreateMutexStatic(&impl->handler_mutex_buf);
    impl->wake          = xSemaphoreCreateBinaryStatic(&impl->wake_buf);
    impl->exited        = xSemaphoreCreateBinaryStatic(&impl->exited_buf);

    if (timebase->external_sync == NULL)
    {
        timebase->external_sync = OS_FreeRTOS_TimeBaseSync;
    }

    impl->task = xTaskCreateStatic(OS_FreeRTOS_TimeBaseTask, timebase->timebase_name,
                                   (uint32_t)(sizeof(impl->stack) / sizeof(impl->stack[0])),
                                   (void *)(uintptr_t)OS_ObjectIdToInteger(id),
                                   OS_FreeRTOS_MapPriority(OS_FREERTOS_TIMEBASE_PRIORITY), impl->stack, &impl->tcb);

    return (impl->task != NULL) ? OS_SUCCESS : OS_TIMER_ERR_INTERNAL;
}

int32 OS_TimeBaseSet_Impl(const OS_object_token_t *token, uint32 start_time, uint32 interval_time)
{
    OS_impl_timebase_internal_record_t *impl     = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    OS_timebase_internal_record_t *     timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);
    TickType_t                          start_ticks;

    if (timebase->external_sync != OS_FreeRTOS_TimeBaseSync)
    {
        return OS_SUCCESS; /* externally synchronized: nothing to arm here */
    }

    start_ticks = (start_time > 0) ? OS_FreeRTOS_UsecToTicks(start_time) : 0;

    taskENTER_CRITICAL();
    impl->first          = true;
    impl->armed          = (start_time > 0);
    impl->interval_ticks = (start_time > 0 && interval_time > 0) ? OS_FreeRTOS_UsecToTicks(interval_time) : 0;
    impl->start_usec     = (uint32)start_ticks * OS_FreeRTOS_UsecPerTick();
    impl->interval_usec  = (uint32)impl->interval_ticks * OS_FreeRTOS_UsecPerTick();
    impl->next_wake      = xTaskGetTickCount() + start_ticks;
    taskEXIT_CRITICAL();

    timebase->accuracy_usec = OS_FreeRTOS_UsecPerTick();

    (void)xSemaphoreGive(impl->wake);
    return OS_SUCCESS;
}

int32 OS_TimeBaseDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    impl->shutdown = true;
    (void)xSemaphoreGive(impl->wake);

    if (xSemaphoreTake(impl->exited, OS_FreeRTOS_MsToTicks(OS_FREERTOS_TIMEBASE_EXIT_WAIT_MS)) != pdTRUE)
    {
        OS_DEBUG("OS_TimeBaseDelete_Impl: helper task did not stop, deleting it\n");
    }

    OS_FreeRTOS_SemCancelTask(impl->task);
    vTaskDelete(impl->task);
    impl->task = NULL;
    vSemaphoreDelete(impl->handler_mutex);
    vSemaphoreDelete(impl->wake);
    vSemaphoreDelete(impl->exited);

    return OS_SUCCESS;
}

int32 OS_TimeBaseGetInfo_Impl(const OS_object_token_t *token, OS_timebase_prop_t *timer_prop)
{
    (void)token;
    (void)timer_prop;
    return OS_SUCCESS;
}
