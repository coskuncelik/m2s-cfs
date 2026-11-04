/*
 * bsp_freertos_hooks.c — m2s-cfs simulator: FreeRTOS configASSERT handler.
 * Built into the kernel library so the kernel has no link-order dependency on the BSP.
 */
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"

void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    fprintf(stderr, "FreeRTOS ASSERT FAILED: %s:%lu\n", pcFile, ulLine);
    abort();
}
