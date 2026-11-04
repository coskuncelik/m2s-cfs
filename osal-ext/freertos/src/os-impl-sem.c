/*
 * os-impl-sem.c — m2s-cfs OSAL FreeRTOS layer: semaphore core used by binary and counting
 * semaphores.
 *
 * Why not a native FreeRTOS semaphore: OSAL needs OS_BinSemFlush() (release every waiter
 * without changing the count), which FreeRTOS does not provide. Here each waiter puts a
 * record on its own stack into a priority-ordered list and sleeps on a task notification.
 * Give hands the count straight to the first (highest priority) waiter; flush wakes all.
 *
 * Concurrency: the list is only touched with the scheduler suspended (vTaskSuspendAll).
 * No ISR uses these semaphores. A waiter's record stays valid until that waiter has seen
 * its result in its own suspended section, and every notify happens inside the giver's
 * suspended section, so a woken waiter always finds its notification already pending.
 */
#include "os-freertos.h"

#define OS_FREERTOS_WAIT_PENDING  0U
#define OS_FREERTOS_WAIT_GRANTED  1U
#define OS_FREERTOS_WAIT_FLUSHED  2U
#define OS_FREERTOS_WAIT_RELEASED 3U

struct OS_FreeRTOS_SemWaiter
{
    OS_FreeRTOS_SemWaiter_t *next;
    OS_FreeRTOS_Sem_t *      sem; /* NULL once removed from the list */
    TaskHandle_t             task;
    UBaseType_t              priority;
    volatile uint8           result;
};

/* Remove a waiter from its list. Scheduler must be suspended. */
static void OS_FreeRTOS_SemUnlink(OS_FreeRTOS_SemWaiter_t *waiter)
{
    OS_FreeRTOS_SemWaiter_t **link;

    if (waiter->sem == NULL)
    {
        return;
    }

    for (link = &waiter->sem->head; *link != NULL; link = &(*link)->next)
    {
        if (*link == waiter)
        {
            *link = waiter->next;
            break;
        }
    }
    waiter->sem  = NULL;
    waiter->next = NULL;
}

/* Wake the first waiter with the given result. Scheduler must be suspended. */
static void OS_FreeRTOS_SemWakeFirst(OS_FreeRTOS_Sem_t *sem, uint8 result)
{
    OS_FreeRTOS_SemWaiter_t *waiter = sem->head;

    sem->head      = waiter->next;
    waiter->next   = NULL;
    waiter->sem    = NULL;
    waiter->result = result;
    (void)xTaskNotifyGiveIndexed(waiter->task, OS_FREERTOS_SEM_NOTIFY_INDEX);
}

void OS_FreeRTOS_SemInit(OS_FreeRTOS_Sem_t *sem, uint32 initial_value, uint32 max_value)
{
    sem->value     = initial_value;
    sem->max_value = max_value;
    sem->head      = NULL;
}

int32 OS_FreeRTOS_SemTake(OS_FreeRTOS_Sem_t *sem, TickType_t ticks)
{
    OS_FreeRTOS_SemWaiter_t   waiter;
    OS_FreeRTOS_SemWaiter_t **link;
    TimeOut_t                 timeout;
    TickType_t                remaining;
    uint32_t                  notified;

    waiter.next     = NULL;
    waiter.sem      = sem;
    waiter.task     = xTaskGetCurrentTaskHandle();
    waiter.priority = uxTaskPriorityGet(NULL);
    waiter.result   = OS_FREERTOS_WAIT_PENDING;

    vTaskSuspendAll();
    if (sem->value > 0)
    {
        --sem->value;
        (void)xTaskResumeAll();
        return OS_SUCCESS;
    }
    if (ticks == 0)
    {
        (void)xTaskResumeAll();
        return OS_SEM_TIMEOUT;
    }
    for (link = &sem->head; *link != NULL && (*link)->priority >= waiter.priority; link = &(*link)->next)
    {
    }
    waiter.next = *link;
    *link       = &waiter;
    vTaskSetThreadLocalStoragePointer(NULL, OS_FREERTOS_TLS_SEM_WAITER, &waiter);
    (void)xTaskResumeAll();

    vTaskSetTimeOutState(&timeout);
    remaining = ticks;
    do
    {
        notified = ulTaskNotifyTakeIndexed(OS_FREERTOS_SEM_NOTIFY_INDEX, pdTRUE, remaining);
    } while (notified != 0 && waiter.result == OS_FREERTOS_WAIT_PENDING &&
             xTaskCheckForTimeOut(&timeout, &remaining) == pdFALSE);

    vTaskSuspendAll();
    vTaskSetThreadLocalStoragePointer(NULL, OS_FREERTOS_TLS_SEM_WAITER, NULL);
    if (waiter.result == OS_FREERTOS_WAIT_PENDING)
    {
        OS_FreeRTOS_SemUnlink(&waiter); /* timed out */
    }
    (void)xTaskResumeAll();

    if (waiter.result == OS_FREERTOS_WAIT_PENDING)
    {
        return OS_SEM_TIMEOUT;
    }

    if (notified == 0)
    {
        /* Woken just as the wait timed out: the notify is already pending; consume it. */
        (void)ulTaskNotifyTakeIndexed(OS_FREERTOS_SEM_NOTIFY_INDEX, pdTRUE, 0);
    }

    return (waiter.result == OS_FREERTOS_WAIT_RELEASED) ? OS_SEM_FAILURE : OS_SUCCESS;
}

int32 OS_FreeRTOS_SemGive(OS_FreeRTOS_Sem_t *sem)
{
    int32 return_code = OS_SUCCESS;

    vTaskSuspendAll();
    if (sem->head != NULL)
    {
        OS_FreeRTOS_SemWakeFirst(sem, OS_FREERTOS_WAIT_GRANTED);
    }
    else if (sem->value < sem->max_value)
    {
        ++sem->value;
    }
    else if (sem->max_value > 1)
    {
        return_code = OS_SEM_FAILURE; /* counting semaphore overflow */
    }
    (void)xTaskResumeAll();

    return return_code;
}

void OS_FreeRTOS_SemFlush(OS_FreeRTOS_Sem_t *sem)
{
    vTaskSuspendAll();
    while (sem->head != NULL)
    {
        OS_FreeRTOS_SemWakeFirst(sem, OS_FREERTOS_WAIT_FLUSHED);
    }
    (void)xTaskResumeAll();
}

void OS_FreeRTOS_SemRelease(OS_FreeRTOS_Sem_t *sem)
{
    vTaskSuspendAll();
    while (sem->head != NULL)
    {
        OS_FreeRTOS_SemWakeFirst(sem, OS_FREERTOS_WAIT_RELEASED);
    }
    sem->value = 0;
    (void)xTaskResumeAll();
}

uint32 OS_FreeRTOS_SemGetValue(OS_FreeRTOS_Sem_t *sem)
{
    uint32 value;

    vTaskSuspendAll();
    value = sem->value;
    (void)xTaskResumeAll();

    return value;
}

void OS_FreeRTOS_SemCancelTask(TaskHandle_t task)
{
    OS_FreeRTOS_SemWaiter_t *waiter;

    vTaskSuspendAll();
    waiter = (OS_FreeRTOS_SemWaiter_t *)pvTaskGetThreadLocalStoragePointer(task, OS_FREERTOS_TLS_SEM_WAITER);
    if (waiter != NULL)
    {
        OS_FreeRTOS_SemUnlink(waiter);
        vTaskSetThreadLocalStoragePointer(task, OS_FREERTOS_TLS_SEM_WAITER, NULL);
    }
    (void)xTaskResumeAll();
}
