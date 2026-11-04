/*
 * os-impl-console.c — m2s-cfs OSAL FreeRTOS layer: console.
 *
 * Output path: OS_printf -> console ring buffer -> OS_ConsoleOutput_Impl (OSAL portable
 * os-impl-console-bsp.c) -> OS_BSP_ConsoleOutput_Impl (BSP). With OSAL_CONFIG_CONSOLE_ASYNC
 * a static utility task drains the buffer when woken.
 */
#include "os-freertos.h"
#include "os-impl-console.h"
#include "os-shared-printf.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

OS_impl_console_internal_record_t OS_impl_console_table[OS_MAX_CONSOLES];

static StaticTask_t OS_FreeRTOS_ConsoleTcb;
static StackType_t  OS_FreeRTOS_ConsoleStack[OS_FREERTOS_CONSOLE_STACK_BYTES / sizeof(StackType_t)];

void OS_ConsoleWakeup_Impl(const OS_object_token_t *token)
{
    OS_impl_console_internal_record_t *local = OS_OBJECT_TABLE_GET(OS_impl_console_table, *token);

    (void)xSemaphoreGive(local->data_sem);
}

static void OS_FreeRTOS_ConsoleTask(void *arg)
{
    OS_object_token_t                  token;
    OS_impl_console_internal_record_t *local;

    if (OS_ObjectIdGetById(OS_LOCK_MODE_REFCOUNT, OS_OBJECT_TYPE_OS_CONSOLE,
                           OS_ObjectIdFromInteger((unsigned long)(uintptr_t)arg), &token) == OS_SUCCESS)
    {
        local = OS_OBJECT_TABLE_GET(OS_impl_console_table, token);
        while (OS_SharedGlobalVars.GlobalState != OS_SHUTDOWN_MAGIC_NUMBER)
        {
            OS_ConsoleOutput_Impl(&token);
            (void)xSemaphoreTake(local->data_sem, portMAX_DELAY);
        }
        OS_ObjectIdRelease(&token);
    }

    for (;;)
    {
        vTaskSuspend(NULL);
    }
}

int32 OS_ConsoleCreate_Impl(const OS_object_token_t *token)
{
    OS_impl_console_internal_record_t *local   = OS_OBJECT_TABLE_GET(OS_impl_console_table, *token);
    OS_console_internal_record_t *     console = OS_OBJECT_TABLE_GET(OS_console_table, *token);

    if (OS_ObjectIndexFromToken(token) != 0)
    {
        return OS_ERR_NOT_IMPLEMENTED; /* one console only */
    }

    local->data_sem = xSemaphoreCreateBinaryStatic(&local->data_sem_buf);
    if (local->data_sem == NULL)
    {
        return OS_SEM_FAILURE;
    }

    if (console->IsAsync)
    {
        if (xTaskCreateStatic(OS_FreeRTOS_ConsoleTask, "OS_CONSOLE",
                              (uint32_t)(sizeof(OS_FreeRTOS_ConsoleStack) / sizeof(OS_FreeRTOS_ConsoleStack[0])),
                              (void *)(uintptr_t)OS_ObjectIdToInteger(OS_ObjectIdFromToken(token)),
                              OS_FreeRTOS_MapPriority(OS_UTILITYTASK_PRIORITY), OS_FreeRTOS_ConsoleStack,
                              &OS_FreeRTOS_ConsoleTcb) == NULL)
        {
            return OS_ERROR;
        }
    }

    return OS_SUCCESS;
}
