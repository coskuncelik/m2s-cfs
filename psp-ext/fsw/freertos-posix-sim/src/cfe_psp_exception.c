/*
 * cfe_psp_exception.c — m2s-cfs freertos-posix-sim PSP: exceptions (not captured on the simulator).
 */
#include "common_types.h"
#include "osapi.h"
#include "cfe_psp.h"
#include "cfe_psp_config.h"
#include "cfe_psp_memory.h"
#include "cfe_psp_exceptionstorage_types.h"
#include "cfe_psp_exceptionstorage_api.h"

void CFE_PSP_AttachExceptions(void)
{
    OS_printf("CFE_PSP: exceptions are not captured on this platform\n");
    CFE_PSP_Exception_Reset();
}

int32 CFE_PSP_ExceptionGetSummary_Impl(const CFE_PSP_Exception_LogData_t *Buffer, char *ReasonBuf, uint32 ReasonSize)
{
    (void)Buffer;
    (void)ReasonBuf;
    (void)ReasonSize;
    return CFE_PSP_ERROR_NOT_IMPLEMENTED;
}

void CFE_PSP_SetDefaultExceptionEnvironment(void) {}
