/*
 * os-impl-tasks.c — m2s-cfs OSAL FreeRTOS layer: tasks, with static TCB and stack pools (D18).
 *
 * Memory: a task takes one TCB slot and, unless the caller supplies a stack, one stack from
 * the smallest stack pool class that fits. Both return to their pools when the task is gone.
 *
 * Task exit: OS_TaskExit_Impl() runs after the shared layer has already freed the OSAL
 * record, and a task cannot release the memory it is running on. So an exiting task marks
 * its TCB slot as a "zombie" and suspends itself. The next OS_TaskCreate_Impl() deletes
 * suspended zombies (deleting another task is immediate in FreeRTOS) and reuses their memory.
 *
 * Task identity: TLS pointer OS_FREERTOS_TLS_TASK_ID holds the OSAL id (set by
 * OS_TaskRegister_Impl, which the shared layer calls in the new task's context).
 */
#include "os-freertos.h"
#include "os-shared-task.h"
#include "os-shared-idmap.h"

/* ---- Stack pools -------------------------------------------------------------------- */

typedef struct
{
    size_t       bytes;
    uint32       count;
    StackType_t *base;   /* count * (bytes / sizeof(StackType_t)) words */
    uint8 *      in_use; /* count flags */
} OS_FreeRTOS_StackClass_t;

#define OS_FREERTOS_DEFINE_STACK_POOL(bytes, count)                                         \
    static StackType_t OS_FreeRTOS_Stack_##bytes[count][(bytes) / sizeof(StackType_t)]; \
    static uint8       OS_FreeRTOS_StackInUse_##bytes[count];
OS_FREERTOS_STACK_POOLS(OS_FREERTOS_DEFINE_STACK_POOL)

#define OS_FREERTOS_STACK_CLASS_ENTRY(bytes, count) \
    {(bytes), (count), &OS_FreeRTOS_Stack_##bytes[0][0], OS_FreeRTOS_StackInUse_##bytes},
static const OS_FreeRTOS_StackClass_t OS_FreeRTOS_StackClasses[] = {OS_FREERTOS_STACK_POOLS(OS_FREERTOS_STACK_CLASS_ENTRY)};

#define OS_FREERTOS_NUM_STACK_CLASSES (sizeof(OS_FreeRTOS_StackClasses) / sizeof(OS_FreeRTOS_StackClasses[0]))

/* ---- TCB slots ---------------------------------------------------------------------- */

#define OS_FREERTOS_SLOT_FREE   0U
#define OS_FREERTOS_SLOT_ACTIVE 1U
#define OS_FREERTOS_SLOT_ZOMBIE 2U

typedef struct
{
    StaticTask_t   tcb;
    TaskHandle_t   handle;
    uint8 *        stack_in_use; /* pool flag to clear on release, or NULL for a caller stack */
    volatile uint8 state;
} OS_FreeRTOS_TaskSlot_t;

static OS_FreeRTOS_TaskSlot_t OS_FreeRTOS_TaskSlots[OS_FREERTOS_MAX_TCBS];

typedef struct
{
    OS_FreeRTOS_TaskSlot_t *slot;
} OS_impl_task_internal_record_t;

static OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];

/* Pool bookkeeping is protected by suspending the scheduler (no ISR involvement). */

static StackType_t *OS_FreeRTOS_StackAlloc(size_t bytes, uint8 **in_use_flag, uint32 *words)
{
    uint32 c;
    uint32 i;

    for (c = 0; c < OS_FREERTOS_NUM_STACK_CLASSES; ++c)
    {
        const OS_FreeRTOS_StackClass_t *cls = &OS_FreeRTOS_StackClasses[c];

        if (cls->bytes < bytes)
        {
            continue;
        }
        for (i = 0; i < cls->count; ++i)
        {
            if (!cls->in_use[i])
            {
                cls->in_use[i] = 1;
                *in_use_flag   = &cls->in_use[i];
                *words         = (uint32)(cls->bytes / sizeof(StackType_t));
                return cls->base + (i * (cls->bytes / sizeof(StackType_t)));
            }
        }
    }
    return NULL;
}

static void OS_FreeRTOS_SlotRelease(OS_FreeRTOS_TaskSlot_t *slot)
{
    if (slot->stack_in_use != NULL)
    {
        *slot->stack_in_use = 0;
        slot->stack_in_use  = NULL;
    }
    slot->handle = NULL;
    slot->state  = OS_FREERTOS_SLOT_FREE;
}

/* Delete exited tasks that have reached vTaskSuspend(NULL) and free their memory. */
static void OS_FreeRTOS_ReapZombies(void)
{
    uint32 i;

    for (i = 0; i < OS_FREERTOS_MAX_TCBS; ++i)
    {
        OS_FreeRTOS_TaskSlot_t *slot = &OS_FreeRTOS_TaskSlots[i];

        if (slot->state == OS_FREERTOS_SLOT_ZOMBIE && eTaskGetState(slot->handle) == eSuspended)
        {
            vTaskDelete(slot->handle);
            vTaskSuspendAll();
            OS_FreeRTOS_SlotRelease(slot);
            (void)xTaskResumeAll();
        }
    }
}

static void OS_FreeRTOS_TaskEntry(void *arg)
{
    OS_TaskEntryPoint(OS_ObjectIdFromInteger((unsigned long)(uintptr_t)arg));
}

/* ---- Implementation API --------------------------------------------------------------- */

int32 OS_FreeRTOS_TaskAPI_Impl_Init(void)
{
    memset(OS_impl_task_table, 0, sizeof(OS_impl_task_table));
    memset(OS_FreeRTOS_TaskSlots, 0, sizeof(OS_FreeRTOS_TaskSlots));
    return OS_SUCCESS;
}

int32 OS_TaskCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);
    OS_task_internal_record_t *     task = OS_OBJECT_TABLE_GET(OS_task_table, *token);
    OS_FreeRTOS_TaskSlot_t *        slot = NULL;
    StackType_t *                   stack;
    uint8 *                         stack_in_use = NULL;
    uint32                          words;
    uint32                          i;
    osal_id_t                       task_id = OS_ObjectIdFromToken(token);

    (void)flags; /* OS_FP_ENABLED: no per-task FP context switch setting in FreeRTOS */

    OS_FreeRTOS_ReapZombies();

    vTaskSuspendAll();
    for (i = 0; i < OS_FREERTOS_MAX_TCBS; ++i)
    {
        if (OS_FreeRTOS_TaskSlots[i].state == OS_FREERTOS_SLOT_FREE)
        {
            slot        = &OS_FreeRTOS_TaskSlots[i];
            slot->state = OS_FREERTOS_SLOT_ACTIVE;
            break;
        }
    }
    if (slot == NULL)
    {
        (void)xTaskResumeAll();
        OS_DEBUG("OS_TaskCreate_Impl: no free TCB slot\n");
        return OS_ERROR;
    }

    if (task->stack_pointer != NULL)
    {
        stack = (StackType_t *)task->stack_pointer;
        words = (uint32)(task->stack_size / sizeof(StackType_t));
    }
    else
    {
        stack = OS_FreeRTOS_StackAlloc(task->stack_size, &stack_in_use, &words);
    }
    if (stack == NULL)
    {
        OS_FreeRTOS_SlotRelease(slot);
        (void)xTaskResumeAll();
        OS_DEBUG("OS_TaskCreate_Impl: no free stack of %lu bytes\n", (unsigned long)task->stack_size);
        return OS_ERROR;
    }
    slot->stack_in_use = stack_in_use;

    /* The new task cannot run before the scheduler resumes, so its TLS id is set first. */
    slot->handle = xTaskCreateStatic(OS_FreeRTOS_TaskEntry, task->task_name, words,
                                     (void *)(uintptr_t)OS_ObjectIdToInteger(task_id),
                                     OS_FreeRTOS_MapPriority(task->priority), stack, &slot->tcb);
    if (slot->handle == NULL)
    {
        OS_FreeRTOS_SlotRelease(slot);
        (void)xTaskResumeAll();
        return OS_ERROR;
    }
    vTaskSetThreadLocalStoragePointer(slot->handle, OS_FREERTOS_TLS_TASK_ID,
                                      (void *)(uintptr_t)OS_ObjectIdToInteger(task_id));
    impl->slot = slot;
    (void)xTaskResumeAll();

    return OS_SUCCESS;
}

int32 OS_TaskDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);
    OS_FreeRTOS_TaskSlot_t *        slot = impl->slot;

    if (slot == NULL)
    {
        return OS_ERROR;
    }
    if (slot->handle == xTaskGetCurrentTaskHandle())
    {
        return OS_ERR_INCORRECT_OBJ_STATE; /* a task ends itself with OS_TaskExit() */
    }

    OS_FreeRTOS_SemCancelTask(slot->handle);
    vTaskDelete(slot->handle);

    vTaskSuspendAll();
    OS_FreeRTOS_SlotRelease(slot);
    impl->slot = NULL;
    (void)xTaskResumeAll();

    return OS_SUCCESS;
}

int32 OS_TaskDetach_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_SUCCESS;
}

void OS_TaskExit_Impl(void)
{
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    uint32       i;

    vTaskSuspendAll();
    for (i = 0; i < OS_FREERTOS_MAX_TCBS; ++i)
    {
        if (OS_FreeRTOS_TaskSlots[i].handle == self && OS_FreeRTOS_TaskSlots[i].state == OS_FREERTOS_SLOT_ACTIVE)
        {
            OS_FreeRTOS_TaskSlots[i].state = OS_FREERTOS_SLOT_ZOMBIE;
            break;
        }
    }
    vTaskSetThreadLocalStoragePointer(NULL, OS_FREERTOS_TLS_TASK_ID, NULL);
    (void)xTaskResumeAll();

    for (;;)
    {
        vTaskSuspend(NULL);
    }
}

int32 OS_TaskDelay_Impl(uint32 millisecond)
{
    vTaskDelay(OS_FreeRTOS_MsToTicks(millisecond));
    return OS_SUCCESS;
}

int32 OS_TaskSetPriority_Impl(const OS_object_token_t *token, osal_priority_t new_priority)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    vTaskPrioritySet(impl->slot->handle, OS_FreeRTOS_MapPriority(new_priority));
    return OS_SUCCESS;
}

int32 OS_TaskMatch_Impl(const OS_object_token_t *token)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    return (impl->slot != NULL && impl->slot->handle == xTaskGetCurrentTaskHandle()) ? OS_SUCCESS : OS_ERROR;
}

int32 OS_TaskRegister_Impl(osal_id_t global_task_id)
{
    vTaskSetThreadLocalStoragePointer(NULL, OS_FREERTOS_TLS_TASK_ID,
                                      (void *)(uintptr_t)OS_ObjectIdToInteger(global_task_id));
    return OS_SUCCESS;
}

osal_id_t OS_TaskGetId_Impl(void)
{
    uintptr_t value;

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return OS_OBJECT_ID_UNDEFINED;
    }
    value = (uintptr_t)pvTaskGetThreadLocalStoragePointer(NULL, OS_FREERTOS_TLS_TASK_ID);
    return (value != 0) ? OS_ObjectIdFromInteger((unsigned long)value) : OS_OBJECT_ID_UNDEFINED;
}

int32 OS_TaskGetInfo_Impl(const OS_object_token_t *token, OS_task_prop_t *task_prop)
{
    (void)token;
    (void)task_prop;
    return OS_SUCCESS; /* the shared layer fills in all fields */
}

int32 OS_TaskValidateSystemData_Impl(const void *sysdata, size_t sysdata_size)
{
    if (sysdata == NULL || sysdata_size != sizeof(TaskHandle_t))
    {
        return OS_INVALID_POINTER;
    }
    return OS_SUCCESS;
}

bool OS_TaskIdMatchSystemData_Impl(void *ref, const OS_object_token_t *token, const OS_common_record_t *obj)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    (void)obj;
    return (impl->slot != NULL && *(const TaskHandle_t *)ref == impl->slot->handle);
}
