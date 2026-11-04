/*
 * cfe_psp_watchdog.c — m2s-cfs freertos-posix-sim PSP: watchdog (none on the simulator; value kept only).
 */
#include "common_types.h"
#include "cfe_psp.h"
#include "cfe_psp_config.h"

static uint32 CFE_PSP_WatchdogValue = 0;

void CFE_PSP_WatchdogInit(void)
{
    CFE_PSP_WatchdogValue = CFE_PSP_WATCHDOG_MAX;
}

void CFE_PSP_WatchdogEnable(void) {}

void CFE_PSP_WatchdogDisable(void) {}

void CFE_PSP_WatchdogService(void) {}

uint32 CFE_PSP_WatchdogGet(void)
{
    return CFE_PSP_WatchdogValue;
}

void CFE_PSP_WatchdogSet(uint32 WatchdogValue)
{
    CFE_PSP_WatchdogValue = WatchdogValue;
}
