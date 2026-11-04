/*
 * os-impl-mutex.c — m2s-cfs OSAL FreeRTOS layer: mutexes.
 * OSAL mutexes are recursive; FreeRTOS static recursive mutexes give priority inheritance.
 */
#include "os-freertos.h"
#include "os-shared-mutex.h"
#include "os-shared-idmap.h"

typedef struct
{
    StaticSemaphore_t buffer;
    SemaphoreHandle_t handle;
} OS_impl_mutex_internal_record_t;

static OS_impl_mutex_internal_record_t OS_impl_mutex_table[OS_MAX_MUTEXES];

int32 OS_FreeRTOS_MutexAPI_Impl_Init(void)
{
    memset(OS_impl_mutex_table, 0, sizeof(OS_impl_mutex_table));
    return OS_SUCCESS;
}

int32 OS_MutSemCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    (void)options;

    impl->handle = xSemaphoreCreateRecursiveMutexStatic(&impl->buffer);
    return (impl->handle != NULL) ? OS_SUCCESS : OS_SEM_FAILURE;
}

int32 OS_MutSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    vSemaphoreDelete(impl->handle);
    impl->handle = NULL;
    return OS_SUCCESS;
}

int32 OS_MutSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    /* Fails if the caller does not hold the mutex. */
    return (xSemaphoreGiveRecursive(impl->handle) == pdTRUE) ? OS_SUCCESS : OS_SEM_FAILURE;
}

int32 OS_MutSemTake_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    return (xSemaphoreTakeRecursive(impl->handle, portMAX_DELAY) == pdTRUE) ? OS_SUCCESS : OS_SEM_FAILURE;
}

int32 OS_MutSemGetInfo_Impl(const OS_object_token_t *token, OS_mut_sem_prop_t *mut_prop)
{
    (void)token;
    (void)mut_prop;
    return OS_SUCCESS; /* the shared layer fills in name and creator */
}
