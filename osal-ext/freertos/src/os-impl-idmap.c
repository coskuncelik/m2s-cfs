/*
 * os-impl-idmap.c — m2s-cfs OSAL FreeRTOS layer: one static mutex per OSAL object type
 * protects that type's global table.
 */
#include "os-freertos.h"
#include "os-shared-idmap.h"

typedef struct
{
    StaticSemaphore_t buffer;
    SemaphoreHandle_t handle;
} OS_impl_objtype_lock_t;

static OS_impl_objtype_lock_t OS_impl_objtype_lock_table[OS_OBJECT_TYPE_USER];

int32 OS_FreeRTOS_TableMutex_Init(osal_objtype_t idtype)
{
    if (idtype >= OS_OBJECT_TYPE_USER)
    {
        return OS_ERROR;
    }

    OS_impl_objtype_lock_table[idtype].handle = xSemaphoreCreateMutexStatic(&OS_impl_objtype_lock_table[idtype].buffer);
    return (OS_impl_objtype_lock_table[idtype].handle != NULL) ? OS_SUCCESS : OS_ERROR;
}

void OS_Lock_Global_Impl(osal_objtype_t idtype)
{
    if (idtype < OS_OBJECT_TYPE_USER && OS_impl_objtype_lock_table[idtype].handle != NULL)
    {
        (void)xSemaphoreTake(OS_impl_objtype_lock_table[idtype].handle, portMAX_DELAY);
    }
}

void OS_Unlock_Global_Impl(osal_objtype_t idtype)
{
    if (idtype < OS_OBJECT_TYPE_USER && OS_impl_objtype_lock_table[idtype].handle != NULL)
    {
        (void)xSemaphoreGive(OS_impl_objtype_lock_table[idtype].handle);
    }
}

void OS_WaitForStateChange_Impl(osal_objtype_t idtype, uint32 attempts)
{
    TickType_t wait_ticks = (attempts <= 10) ? (TickType_t)(attempts * attempts) : (TickType_t)100;

    OS_Unlock_Global_Impl(idtype);
    vTaskDelay(wait_ticks);
    OS_Lock_Global_Impl(idtype);
}
