/*
 * bsp_start.c — m2s-cfs OSAL BSP "freertos-posix-sim": entry point and BSP services.
 *
 * main() creates one static BSP task and starts the FreeRTOS scheduler. The BSP task runs
 * OS_Application_Startup() and OS_Application_Run() (OSAL needs a running scheduler, so
 * neither may run before it), then exits the process with the application status.
 */
#define _GNU_SOURCE /* sched_setaffinity, sched_getcpu */
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "osapi.h"
#include "bsp-impl.h"

/* Main task: near the top of the OSAL range (like the RTEMS init task, priority 10 of 255). */
#define BSP_MAIN_TASK_PRIORITY    (configMAX_PRIORITIES - 3)
#define BSP_MAIN_TASK_STACK_BYTES 16384

static StaticTask_t      OS_BSP_MainTcb;
static StackType_t       OS_BSP_MainStack[BSP_MAIN_TASK_STACK_BYTES / sizeof(StackType_t)];
static StaticSemaphore_t OS_BSP_AccessMutexBuf;
static SemaphoreHandle_t OS_BSP_AccessMutex;

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
    abort(); /* same as OSAL's generic-linux BSP */
}

static int OS_BSP_GetReturnStatus(void)
{
    return (OS_BSP_Global.AppStatus == OS_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void OS_BSP_MainTask(void *arg)
{
    (void)arg;

    OS_Application_Startup();
    OS_Application_Run();

    /* Simulator: end the host process with the application status. */
    exit(OS_BSP_GetReturnStatus());
}

/*
 * Pin the whole simulator to one host CPU (all threads inherit this). FreeRTOS here is a
 * single-core kernel and the POSIX port runs one task thread at a time, so nothing is lost.
 * Without it, task threads bouncing between vCPUs on the development VM starved the tick
 * thread for tens of seconds (measured in step 04a; see docs/DECISIONS.md D21).
 */
static void OS_BSP_PinToOneCpu(void)
{
    cpu_set_t set;
    int       cpu = sched_getcpu();

    CPU_ZERO(&set);
    CPU_SET((cpu >= 0) ? cpu : 0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
    {
        fprintf(stderr, "BSP: warning: cannot pin to one CPU, timing may be unreliable\n");
    }
}

int main(int argc, char *argv[])
{
    OS_BSP_PinToOneCpu();

    memset(&OS_BSP_Global, 0, sizeof(OS_BSP_Global));
    OS_BSP_Global.ArgC = (uint32)argc;
    OS_BSP_Global.ArgV = argv;

    OS_BSP_AccessMutex = xSemaphoreCreateRecursiveMutexStatic(&OS_BSP_AccessMutexBuf);

    if (xTaskCreateStatic(OS_BSP_MainTask, "BSP_MAIN", BSP_MAIN_TASK_STACK_BYTES / sizeof(StackType_t), NULL,
                          BSP_MAIN_TASK_PRIORITY, OS_BSP_MainStack, &OS_BSP_MainTcb) == NULL)
    {
        fprintf(stderr, "BSP: cannot create main task\n");
        return EXIT_FAILURE;
    }

    vTaskStartScheduler();

    fprintf(stderr, "BSP: scheduler ended unexpectedly\n");
    return EXIT_FAILURE;
}
