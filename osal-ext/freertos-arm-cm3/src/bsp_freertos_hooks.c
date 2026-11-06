/*
 * bsp_freertos_hooks.c — m2s-cfs Cortex-M3: FreeRTOS assert and stack-overflow handlers.
 * Built into the kernel library so the kernel has no link-order dependency on the BSP.
 * Both record where they were called (for a debugger), print one "M2S ..." line on the board console and
 * stop the CPU with interrupts off (m2s_cm3_diag.h).
 */
#include "FreeRTOS.h"
#include "task.h"

#include "m2s_cm3_diag.h"

volatile const char   *M2S_AssertFile;
volatile unsigned long M2S_AssertLine;
volatile const char   *M2S_StackOverflowTask;

void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    taskDISABLE_INTERRUPTS();
    M2S_AssertFile = pcFile;
    M2S_AssertLine = ulLine;
    M2S_Diag_Write("\nM2S ASSERT ");
    M2S_Diag_Write(pcFile);
    M2S_Diag_Write(":");
    M2S_Diag_WriteDec((uint32_t)ulLine);
    M2S_Diag_Write("\n");
    M2S_Halt();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    M2S_StackOverflowTask = pcTaskName;
    M2S_Diag_Write("\nM2S TRAP stack overflow in task ");
    M2S_Diag_Write(pcTaskName);
    M2S_Diag_Write("\n");
    M2S_Halt();
}
