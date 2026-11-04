/*
 * cfe_psp_ssr.c — m2s-cfs freertos-posix-sim PSP: solid state recorder (none).
 */
#include "common_types.h"
#include "cfe_psp.h"

int32 CFE_PSP_InitSSR(uint32 bus, uint32 device, char *DeviceName)
{
    (void)bus;
    (void)device;
    (void)DeviceName;
    return CFE_PSP_ERROR_NOT_IMPLEMENTED;
}
