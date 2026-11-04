/*
 * os-freertos.h — m2s-cfs OSAL implementation layer for FreeRTOS (internal header).
 *
 * Written for the pinned OSAL (a77b5bd) and FreeRTOS V11.3.2. Static allocation only:
 * every FreeRTOS object is created with a *Static API from memory defined in this layer.
 * See docs/DECISIONS.md D17-D19.
 */
#ifndef OS_FREERTOS_H
#define OS_FREERTOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "osconfig.h"
#include "common_types.h"
#include "osapi-constants.h"
#include "osapi-error.h"
#include "osapi-idmap.h"
#include "osapi-task.h"
#include "osapi-clock.h"
#include "os-shared-globaldefs.h"

#if (configSUPPORT_STATIC_ALLOCATION != 1)
#error "OSAL FreeRTOS layer requires configSUPPORT_STATIC_ALLOCATION 1"
#endif
#if (configNUM_THREAD_LOCAL_STORAGE_POINTERS < 2)
#error "OSAL FreeRTOS layer requires configNUM_THREAD_LOCAL_STORAGE_POINTERS >= 2"
#endif
#if (configTASK_NOTIFICATION_ARRAY_ENTRIES < 2)
#error "OSAL FreeRTOS layer requires configTASK_NOTIFICATION_ARRAY_ENTRIES >= 2"
#endif
#if (configMAX_PRIORITIES < 4)
#error "OSAL FreeRTOS layer requires configMAX_PRIORITIES >= 4"
#endif

/* ---------------------------------------------------------------------------------------
 * Static memory configuration (override with -D on the compiler command line)
 * --------------------------------------------------------------------------------------- */

/*
 * Task stack pools: X(bytes, count), ascending by size. A task gets the smallest free
 * stack whose size is >= the requested stack size (D18).
 */
#ifndef OS_FREERTOS_STACK_POOLS
#define OS_FREERTOS_STACK_POOLS(X) X(4096, 16) X(8192, 16) X(16384, 8) X(32768, 4)
#endif

/* Task control blocks: OSAL tasks plus room for exited tasks not yet reclaimed. */
#ifndef OS_FREERTOS_MAX_TCBS
#define OS_FREERTOS_MAX_TCBS (OS_MAX_TASKS + 8)
#endif

/* Per-queue storage: (depth + 2) * (sizeof(size_t) + message size, rounded up) must fit. */
#ifndef OS_FREERTOS_QUEUE_STORAGE_BYTES
#define OS_FREERTOS_QUEUE_STORAGE_BYTES 8192
#endif

/* Stack of each timebase helper task and of the console task. */
#ifndef OS_FREERTOS_TIMEBASE_STACK_BYTES
#define OS_FREERTOS_TIMEBASE_STACK_BYTES 8192
#endif
#ifndef OS_FREERTOS_CONSOLE_STACK_BYTES
#define OS_FREERTOS_CONSOLE_STACK_BYTES OS_UTILITYTASK_STACK_SIZE
#endif

/* OSAL priority of the timebase helper tasks (0 = highest, as in the POSIX implementation). */
#ifndef OS_FREERTOS_TIMEBASE_PRIORITY
#define OS_FREERTOS_TIMEBASE_PRIORITY 0
#endif

/* How long timebase delete waits for its helper task to stop by itself. */
#ifndef OS_FREERTOS_TIMEBASE_EXIT_WAIT_MS
#define OS_FREERTOS_TIMEBASE_EXIT_WAIT_MS 1000
#endif

/* Thread-local storage and notification slots used by this layer. */
#define OS_FREERTOS_TLS_TASK_ID     0
#define OS_FREERTOS_TLS_SEM_WAITER  1
#define OS_FREERTOS_SEM_NOTIFY_INDEX 1

/* ---------------------------------------------------------------------------------------
 * Helpers (os-impl-common.c)
 * --------------------------------------------------------------------------------------- */

/* OSAL priority (0 = highest .. 255 = lowest) -> FreeRTOS priority (1 .. configMAX_PRIORITIES-2). */
UBaseType_t OS_FreeRTOS_MapPriority(osal_priority_t priority);

/* Milliseconds -> ticks for a timeout: rounded up, plus one tick so the wait is never shorter
 * than requested (a FreeRTOS wait of N ticks can end up to one tick early). 0 -> 0. */
TickType_t OS_FreeRTOS_MsToTicks(uint32 msecs);

/* ---------------------------------------------------------------------------------------
 * Semaphore core (os-impl-sem.c): counting semaphore with a priority-ordered waiter list.
 * A give hands the count directly to the highest-priority waiter; a flush releases all
 * waiters. Waiter records live on the waiting task's stack; tasks are woken with task
 * notifications (index OS_FREERTOS_SEM_NOTIFY_INDEX). No dynamic memory.
 * --------------------------------------------------------------------------------------- */
typedef struct OS_FreeRTOS_SemWaiter OS_FreeRTOS_SemWaiter_t;

typedef struct
{
    uint32                   value;
    uint32                   max_value;
    OS_FreeRTOS_SemWaiter_t *head; /* highest priority first, FIFO within a priority */
} OS_FreeRTOS_Sem_t;

void   OS_FreeRTOS_SemInit(OS_FreeRTOS_Sem_t *sem, uint32 initial_value, uint32 max_value);
int32  OS_FreeRTOS_SemTake(OS_FreeRTOS_Sem_t *sem, TickType_t ticks); /* portMAX_DELAY = forever */
int32  OS_FreeRTOS_SemGive(OS_FreeRTOS_Sem_t *sem);
void   OS_FreeRTOS_SemFlush(OS_FreeRTOS_Sem_t *sem);
void   OS_FreeRTOS_SemRelease(OS_FreeRTOS_Sem_t *sem); /* on delete: waiters return OS_SEM_FAILURE */
uint32 OS_FreeRTOS_SemGetValue(OS_FreeRTOS_Sem_t *sem);
void   OS_FreeRTOS_SemCancelTask(TaskHandle_t task); /* before deleting a task that may be waiting */

/* ---------------------------------------------------------------------------------------
 * Per-module init (called from OS_API_Impl_Init)
 * --------------------------------------------------------------------------------------- */
int32 OS_FreeRTOS_TableMutex_Init(osal_objtype_t idtype);
int32 OS_FreeRTOS_TaskAPI_Impl_Init(void);
int32 OS_FreeRTOS_QueueAPI_Impl_Init(void);
int32 OS_FreeRTOS_BinSemAPI_Impl_Init(void);
int32 OS_FreeRTOS_CountSemAPI_Impl_Init(void);
int32 OS_FreeRTOS_MutexAPI_Impl_Init(void);
int32 OS_FreeRTOS_TimeBaseAPI_Impl_Init(void);

#endif /* OS_FREERTOS_H */
