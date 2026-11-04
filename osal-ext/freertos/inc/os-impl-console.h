/*
 * os-impl-console.h — m2s-cfs OSAL FreeRTOS layer: console record (used by
 * OSAL's portable/os-impl-console-bsp.c, which only needs this header to exist).
 */
#ifndef OS_IMPL_CONSOLE_H
#define OS_IMPL_CONSOLE_H

#include "os-freertos.h"

typedef struct
{
    StaticSemaphore_t data_sem_buf;
    SemaphoreHandle_t data_sem;
} OS_impl_console_internal_record_t;

extern OS_impl_console_internal_record_t OS_impl_console_table[OS_MAX_CONSOLES];

#endif /* OS_IMPL_CONSOLE_H */
