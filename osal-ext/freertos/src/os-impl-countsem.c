/*
 * os-impl-countsem.c — m2s-cfs OSAL FreeRTOS layer: counting semaphores (on the semaphore core).
 */
#include "os-freertos.h"
#include "os-shared-countsem.h"
#include "os-shared-idmap.h"

#define OS_FREERTOS_COUNTSEM_MAX ((uint32)INT32_MAX)

static OS_FreeRTOS_Sem_t OS_impl_count_sem_table[OS_MAX_COUNT_SEMAPHORES];

int32 OS_FreeRTOS_CountSemAPI_Impl_Init(void)
{
    memset(OS_impl_count_sem_table, 0, sizeof(OS_impl_count_sem_table));
    return OS_SUCCESS;
}

int32 OS_CountSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    (void)options;

    if (sem_initial_value > OS_FREERTOS_COUNTSEM_MAX)
    {
        return OS_INVALID_SEM_VALUE;
    }

    OS_FreeRTOS_SemInit(OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token), sem_initial_value,
                        OS_FREERTOS_COUNTSEM_MAX);
    return OS_SUCCESS;
}

int32 OS_CountSemDelete_Impl(const OS_object_token_t *token)
{
    OS_FreeRTOS_SemRelease(OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token));
    return OS_SUCCESS;
}

int32 OS_CountSemGive_Impl(const OS_object_token_t *token)
{
    return OS_FreeRTOS_SemGive(OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token));
}

int32 OS_CountSemTake_Impl(const OS_object_token_t *token)
{
    return OS_FreeRTOS_SemTake(OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token), portMAX_DELAY);
}

int32 OS_CountSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    return OS_FreeRTOS_SemTake(OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token), OS_FreeRTOS_MsToTicks(msecs));
}

int32 OS_CountSemGetInfo_Impl(const OS_object_token_t *token, OS_count_sem_prop_t *count_prop)
{
    count_prop->value = (int32)OS_FreeRTOS_SemGetValue(OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token));
    return OS_SUCCESS;
}
