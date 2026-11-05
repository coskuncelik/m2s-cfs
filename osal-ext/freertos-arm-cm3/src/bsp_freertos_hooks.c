/*
 * bsp_freertos_hooks.c — m2s-cfs Cortex-M3: FreeRTOS assert and stack-overflow handlers.
 * Built into the kernel library so the kernel has no link-order dependency on the BSP.
 * Both record where they were called and stop the CPU with interrupts off, for a debugger.
 */
#include "FreeRTOS.h"
#include "task.h"

volatile const char   *M2S_AssertFile;
volatile unsigned long M2S_AssertLine;
volatile const char   *M2S_StackOverflowTask;

void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    taskDISABLE_INTERRUPTS();
    M2S_AssertFile = pcFile;
    M2S_AssertLine = ulLine;
    for (;;)
    {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    M2S_StackOverflowTask = pcTaskName;
    for (;;)
    {
    }
}
