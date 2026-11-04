/*
 * FreeRTOSConfig.h — m2s-cfs, Linux simulator (FreeRTOS V11.3.2 GCC/Posix port).
 *
 * Static allocation only: configSUPPORT_DYNAMIC_ALLOCATION is 0 and no
 * portable/MemMang/heap_*.c is linked. Any use of pvPortMalloc() is a link error.
 *
 * Note (host side): the POSIX port itself uses glibc malloc() and glibc-managed
 * pthread stacks (port.c, utils/wait_for_event.c). That is host-only and is
 * not evidence about the embedded target (see docs/DECISIONS.md D4).
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Scheduler */
#define configUSE_PREEMPTION                       1
#define configUSE_TIME_SLICING                     1
#define configTICK_RATE_HZ                         ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                       32  /* OSAL 0..255 maps to 1..30; 31 = timer task */
#define configMAX_TASK_NAME_LEN                    16
#define configUSE_16_BIT_TICKS                     0
#define configIDLE_SHOULD_YIELD                    1

/*
 * In the POSIX port the FreeRTOS stack only holds the port's Thread_t record;
 * the task really runs on its pthread stack. Value in words (StackType_t).
 */
#define configMINIMAL_STACK_SIZE                   ( ( configSTACK_DEPTH_TYPE ) 1024 )

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

/* Software timers (needed later by the OSAL timebase) */
#define configUSE_TIMERS                           1
#define configTIMER_TASK_PRIORITY                  ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                   10
#define configTIMER_TASK_STACK_DEPTH               configMINIMAL_STACK_SIZE

/* Hooks */
#define configUSE_IDLE_HOOK                        1   /* sleeps; see freertos_posix_hooks.c */
#define configUSE_TICK_HOOK                        0
#define configUSE_MALLOC_FAILED_HOOK               0
#define configCHECK_FOR_STACK_OVERFLOW             0   /* meaningless on the POSIX port (pthread stacks) */

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

/* Assertions: report and abort the process */
void vAssertCalled( const char * pcFile, unsigned long ulLine );
#define configASSERT( x )    do { if( ( x ) == 0 ) { vAssertCalled( __FILE__, __LINE__ ); } } while( 0 )

#endif /* FREERTOS_CONFIG_H */
