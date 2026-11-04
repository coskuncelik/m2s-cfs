/*
 * cfe_psp_support.c — m2s-cfs freertos-posix-sim PSP: restart, panic, caches, IDs.
 *
 * Simulator: a restart or a panic ends the host process (the next start is a power-on reset).
 */
#include <stdio.h>
#include <stdlib.h>

#include "common_types.h"
#include "osapi.h"
#include "cfe_psp.h"
#include "target_config.h"

#define CFE_PSP_CPU_ID        (GLOBAL_CONFIGDATA.Default_CpuId)
#define CFE_PSP_CPU_NAME      (GLOBAL_CONFIGDATA.Default_CpuName)
#define CFE_PSP_SPACECRAFT_ID (GLOBAL_CONFIGDATA.Default_SpacecraftId)

void CFE_PSP_Restart(uint32 resetType)
{
    OS_printf("CFE_PSP: restart requested (type %u); the simulator exits, the next start is a power-on reset\n",
              (unsigned int)resetType);
    OS_TaskDelay(100); /* let the console task print */
    exit(EXIT_SUCCESS);
}

void CFE_PSP_Panic(int32 ErrorCode)
{
    /* stdio, not OS_printf: the OSAL console may not be running. Host-only path. */
    printf("CFE_PSP: panic, error code 0x%08X. Exiting.\n", (unsigned int)ErrorCode);
    fflush(stdout);
    exit(EXIT_FAILURE);
}

void CFE_PSP_FlushCaches(uint32 type, void *address, uint32 size)
{
    (void)type;
    (void)address;
    (void)size; /* no caches to flush on the simulator */
}

uint32 CFE_PSP_GetProcessorId(void)
{
    return CFE_PSP_CPU_ID;
}

uint32 CFE_PSP_GetSpacecraftId(void)
{
    return CFE_PSP_SPACECRAFT_ID;
}

const char *CFE_PSP_GetProcessorName(void)
{
    return CFE_PSP_CPU_NAME;
}
