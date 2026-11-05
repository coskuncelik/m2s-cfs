/*
 * bsp_start.c — m2s-cfs OSAL BSP "freertos-arm-cm3": entry point and BSP services.
 *
 * main() (called by the board's reset handler) sets up the board, creates one static BSP task and
 * starts the FreeRTOS scheduler. The BSP task runs OS_Application_Startup() and OS_Application_Run()
 * (OSAL needs a running scheduler, so neither may run before it). There is no process to exit: if the
 * application returns, the CPU stops (M2S_BSP_Stop).
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi.h"
#include "bsp-impl.h"
#include "m2s_board.h"

/* Main task: near the top of the OSAL range, as on the simulator. */
#define BSP_MAIN_TASK_PRIORITY    (configMAX_PRIORITIES - 3)
#define BSP_MAIN_TASK_STACK_BYTES 16384

static StaticTask_t      OS_BSP_MainTcb;
static StackType_t       OS_BSP_MainStack[BSP_MAIN_TASK_STACK_BYTES / sizeof(StackType_t)];
static StaticSemaphore_t OS_BSP_AccessMutexBuf;
static SemaphoreHandle_t OS_BSP_AccessMutex;

/* Why the CPU stopped, for a debugger (see also vAssertCalled in bsp_freertos_hooks.c). */
volatile const char *M2S_BSP_StopReason;

void M2S_BSP_Stop(const char *reason);

void M2S_BSP_Stop(const char *reason)
{
    M2S_BSP_StopReason = reason;
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

void OS_BSP_Lock_Impl(void)
{
    if (OS_BSP_AccessMutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        (void)xSemaphoreTakeRecursive(OS_BSP_AccessMutex, portMAX_DELAY);
    }
}

void OS_BSP_Unlock_Impl(void)
{
    if (OS_BSP_AccessMutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        (void)xSemaphoreGiveRecursive(OS_BSP_AccessMutex);
    }
}

void OS_BSP_Shutdown_Impl(void)
{
    M2S_BSP_Stop("OS_BSP_Shutdown_Impl");
}

static void OS_BSP_MainTask(void *arg)
{
    (void)arg;

    OS_Application_Startup();
    OS_Application_Run();

    M2S_BSP_Stop((OS_BSP_Global.AppStatus == OS_SUCCESS) ? "application ended" : "application failed");
}

int main(void)
{
    M2S_Board_Init();

    /* Unbuffered stdout before anything prints: newlib then uses its built-in one-byte buffer instead of
     * allocating one, so the stock PSP's printf/puts go straight to _write -> board console. */
    (void)setvbuf(stdout, NULL, _IONBF, 0);

    memset(&OS_BSP_Global, 0, sizeof(OS_BSP_Global));

    OS_BSP_AccessMutex = xSemaphoreCreateRecursiveMutexStatic(&OS_BSP_AccessMutexBuf);

    if (xTaskCreateStatic(OS_BSP_MainTask, "BSP_MAIN", BSP_MAIN_TASK_STACK_BYTES / sizeof(StackType_t), NULL,
                          BSP_MAIN_TASK_PRIORITY, OS_BSP_MainStack, &OS_BSP_MainTcb) == NULL)
    {
        M2S_BSP_Stop("cannot create BSP_MAIN");
    }

    vTaskStartScheduler();

    M2S_BSP_Stop("scheduler ended");
    return 0;
}
