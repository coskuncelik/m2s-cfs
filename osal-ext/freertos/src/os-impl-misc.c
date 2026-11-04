/*
 * os-impl-misc.c — m2s-cfs OSAL FreeRTOS layer: error names and heap info.
 */
#include "os-freertos.h"
#include "os-shared-errors.h"
#include "os-shared-heap.h"

/* No implementation-specific error codes. */
const OS_ErrorTable_Entry_t OS_IMPL_ERROR_NAME_TABLE[] = {{0, NULL}};

/* There is no heap on this platform (static allocation only). */
int32 OS_HeapGetInfo_Impl(OS_heap_prop_t *heap_prop)
{
    (void)heap_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}
