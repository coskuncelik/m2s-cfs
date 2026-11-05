/*
 * FreeRTOSConfig.h — m2s-cfs, ARM Cortex-M3 (FreeRTOS V11.3.2 GCC/ARM_CM3 port), step 08.
 *
 * Same scheduler, memory and OSAL-related settings as the simulator (config/freertos/posix), plus the
 * Cortex-M3 interrupt settings. Board values (CPU clock, NVIC priority bits) come from m2s_board.h of the
 * selected board (boards/<board>/).
 *
 * Static allocation only: configSUPPORT_DYNAMIC_ALLOCATION is 0 and no portable/MemMang/heap_*.c is
 * linked. Any use of pvPortMalloc() is a link error.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "m2s_board.h"

/* Scheduler */
#define configUSE_PREEMPTION                       1
#define configUSE_TIME_SLICING                     1
#define configCPU_CLOCK_HZ                         ( M2S_BOARD_CPU_CLOCK_HZ )
#define configTICK_RATE_HZ                         ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                       32  /* OSAL 0..255 maps to 1..30; 31 = timer task */
#define configMAX_TASK_NAME_LEN                    16
#define configUSE_16_BIT_TICKS                     0
#define configIDLE_SHOULD_YIELD                    1

/* Idle and timer task stacks (kernel-provided static memory), in words: 1 KiB. */
#define configMINIMAL_STACK_SIZE                   ( ( configSTACK_DEPTH_TYPE ) 256 )

/* Memory allocation: static only */
#define configSUPPORT_STATIC_ALLOCATION            1
#define configSUPPORT_DYNAMIC_ALLOCATION           0
#define configKERNEL_PROVIDED_STATIC_MEMORY        1   /* idle and timer task buffers */

/* OSAL FreeRTOS layer (osal-ext/freertos): TLS[0] = OSAL task id, TLS[1] = semaphore wait record;
 * notification index 1 is used by the OSAL semaphore core (index 0 stays free for the kernel). */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS    2
#define configUSE_TASK_NOTIFICATIONS               1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES      2

/* Synchronization primitives */
#define configUSE_MUTEXES                          1
#define configUSE_RECURSIVE_MUTEXES                1
#define configUSE_COUNTING_SEMAPHORES              1
#define configQUEUE_REGISTRY_SIZE                  0

/* Software timers (the OSAL layer does not use them; kept as on the simulator) */
#define configUSE_TIMERS                           1
#define configTIMER_TASK_PRIORITY                  ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                   10
#define configTIMER_TASK_STACK_DEPTH               configMINIMAL_STACK_SIZE

/* Hooks */
#define configUSE_IDLE_HOOK                        0
#define configUSE_TICK_HOOK                        0
#define configUSE_MALLOC_FAILED_HOOK               0
#define configCHECK_FOR_STACK_OVERFLOW             2   /* real task stacks on the target */

/*
 * Cortex-M3 interrupt priorities. The NVIC uses the top M2S_BOARD_NVIC_PRIO_BITS bits of each 8-bit
 * priority. The kernel (SysTick, PendSV) runs at the lowest priority; interrupts that call FreeRTOS
 * "FromISR" APIs must have a numerically equal or higher priority value than
 * configMAX_SYSCALL_INTERRUPT_PRIORITY (that is, a logically lower or equal urgency).
 */
#define configPRIO_BITS                            M2S_BOARD_NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY    ( ( 1U << configPRIO_BITS ) - 1U )
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#define configKERNEL_INTERRUPT_PRIORITY            ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8U - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY       ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8U - configPRIO_BITS ) )

/* Optional API */
#define INCLUDE_vTaskDelay                         1
#define INCLUDE_xTaskDelayUntil                    1
#define INCLUDE_vTaskDelete                        1
#define INCLUDE_vTaskSuspend                       1
#define INCLUDE_xTaskGetCurrentTaskHandle          1
#define INCLUDE_xTaskGetSchedulerState             1
#define INCLUDE_uxTaskPriorityGet                  1
#define INCLUDE_vTaskPrioritySet                   1
#define INCLUDE_eTaskGetState                      1

/* Assertions: record the location and stop (osal-ext/freertos-arm-cm3/src/bsp_freertos_hooks.c) */
void vAssertCalled( const char * pcFile, unsigned long ulLine );
#define configASSERT( x )    do { if( ( x ) == 0 ) { vAssertCalled( __FILE__, __LINE__ ); } } while( 0 )

#endif /* FREERTOS_CONFIG_H */
