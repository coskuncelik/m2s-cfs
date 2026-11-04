/*
 * os-impl-binsem.c — m2s-cfs OSAL FreeRTOS layer: binary semaphores (on the semaphore core).
 */
#include "os-freertos.h"
#include "os-shared-binsem.h"
#include "os-shared-idmap.h"

static OS_FreeRTOS_Sem_t OS_impl_bin_sem_table[OS_MAX_BIN_SEMAPHORES];

int32 OS_FreeRTOS_BinSemAPI_Impl_Init(void)
{
    memset(OS_impl_bin_sem_table, 0, sizeof(OS_impl_bin_sem_table));
    return OS_SUCCESS;
}

int32 OS_BinSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    (void)options;

    /* OSAL treats any nonzero initial value as "full". */
    OS_FreeRTOS_SemInit(OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token), (sem_initial_value != 0) ? 1 : 0, 1);
    return OS_SUCCESS;
}

int32 OS_BinSemDelete_Impl(const OS_object_token_t *token)
{
    OS_FreeRTOS_SemRelease(OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token));
    return OS_SUCCESS;
}

int32 OS_BinSemGive_Impl(const OS_object_token_t *token)
{
    return OS_FreeRTOS_SemGive(OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token));
}

int32 OS_BinSemFlush_Impl(const OS_object_token_t *token)
{
    OS_FreeRTOS_SemFlush(OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token));
    return OS_SUCCESS;
}

int32 OS_BinSemTake_Impl(const OS_object_token_t *token)
{
    return OS_FreeRTOS_SemTake(OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token), portMAX_DELAY);
}

int32 OS_BinSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    return OS_FreeRTOS_SemTake(OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token), OS_FreeRTOS_MsToTicks(msecs));
}

int32 OS_BinSemGetInfo_Impl(const OS_object_token_t *token, OS_bin_sem_prop_t *bin_prop)
{
    bin_prop->value = (int32)OS_FreeRTOS_SemGetValue(OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token));
    return OS_SUCCESS;
}
