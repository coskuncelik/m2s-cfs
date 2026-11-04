/*
 * FreeRTOS V11.3.2 POSIX-port smoke test for m2s-cfs (no cFE, no OSAL).
 *
 *  - All kernel objects are created statically (no FreeRTOS heap is linked).
 *  - "ticker" (higher priority) prints once per second via xTaskDelayUntil() and
 *    measures each period with the host monotonic clock.
 *  - "worker" (lower priority) prints a heartbeat every 500 ms.
 *  - stdout is serialized with a FreeRTOS mutex, as port.c requires.
 *  - After TICKS periods the ticker checks timing, prints RESULT and ends the scheduler.
 *
 * Exit status: 0 = PASS, 1 = timing FAIL, 2 = assertion.
 *
 * Build with -DSMOKE_FORCE_HEAP_USE to reference pvPortMalloc(). That build must
 * FAIL TO LINK (negative test proving that no FreeRTOS heap is available).
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define TICKS            5
#define PERIOD_MS        1000
#define PERIOD_TOL_MS    150   /* per 1 s period (host wake-up jitter on the VM, D16) */
#define TOTAL_TOL_MS     200   /* whole run; ticks no longer drift (patch 0001) */
#define WORKER_PERIOD_MS 500

static StaticTask_t      xTickerTcb;
static StackType_t       xTickerStack[ configMINIMAL_STACK_SIZE ];
static StaticTask_t      xWorkerTcb;
static StackType_t       xWorkerStack[ configMINIMAL_STACK_SIZE ];
static StaticSemaphore_t xPrintMutexBuf;
static SemaphoreHandle_t xPrintMutex;

static volatile int iExitStatus = 1;

void vAssertCalled( const char * pcFile, unsigned long ulLine )
{
    fprintf( stderr, "ASSERT FAILED: %s:%lu\n", pcFile, ulLine );
    fflush( stderr );
    exit( 2 );
}

static long long llNowMs( void )
{
    struct timespec t;
    clock_gettime( CLOCK_MONOTONIC, &t );
    return ( long long ) t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
}

#define SAFE_PRINT( ... )                                       \
    do {                                                        \
        xSemaphoreTake( xPrintMutex, portMAX_DELAY );           \
        printf( __VA_ARGS__ );                                  \
        fflush( stdout );                                       \
        xSemaphoreGive( xPrintMutex );                          \
    } while( 0 )

static void vTickerTask( void * pvParameters )
{
    ( void ) pvParameters;
    long long llStamp[ TICKS + 1 ];
    TickType_t xLastWake = xTaskGetTickCount();
    int i;
    int iFail = 0;

    llStamp[ 0 ] = llNowMs();

    for( i = 1; i <= TICKS; i++ )
    {
        xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( PERIOD_MS ) );
        llStamp[ i ] = llNowMs();
        SAFE_PRINT( "ticker: %d\n", i );
    }

    for( i = 1; i <= TICKS; i++ )
    {
        long long llDelta = llStamp[ i ] - llStamp[ i - 1 ];
        int iOk = ( llDelta >= PERIOD_MS - PERIOD_TOL_MS ) && ( llDelta <= PERIOD_MS + PERIOD_TOL_MS );
        iFail |= !iOk;
        SAFE_PRINT( "period %d: %lld ms %s\n", i, llDelta, iOk ? "ok" : "OUT OF TOLERANCE" );
    }

    {
        long long llTotal = llStamp[ TICKS ] - llStamp[ 0 ];
        long long llExpected = ( long long ) TICKS * PERIOD_MS;
        int iOk = ( llTotal >= llExpected - TOTAL_TOL_MS ) && ( llTotal <= llExpected + TOTAL_TOL_MS );
        iFail |= !iOk;
        SAFE_PRINT( "total: %lld ms (expected %lld +/- %d) %s\n", llTotal, llExpected, TOTAL_TOL_MS,
                    iOk ? "ok" : "OUT OF TOLERANCE" );
    }

    iExitStatus = iFail ? 1 : 0;
    SAFE_PRINT( "RESULT: %s\n", iFail ? "FAIL" : "PASS" );
    vTaskEndScheduler();
    for( ; ; )
    {
    }
}

static void vWorkerTask( void * pvParameters )
{
    ( void ) pvParameters;
    unsigned uCount = 0;

    for( ; ; )
    {
        vTaskDelay( pdMS_TO_TICKS( WORKER_PERIOD_MS ) );
        SAFE_PRINT( "worker: heartbeat %u\n", ++uCount );
    }
}

int main( void )
{
#ifdef SMOKE_FORCE_HEAP_USE
    /* Negative test only: must not link, since no heap_*.c is built. */
    ( void ) pvPortMalloc( 16 );
#endif

    printf( "FreeRTOS %s POSIX smoke test (static allocation only)\n", tskKERNEL_VERSION_NUMBER );
    fflush( stdout );

    xPrintMutex = xSemaphoreCreateMutexStatic( &xPrintMutexBuf );
    configASSERT( xPrintMutex != NULL );

    configASSERT( xTaskCreateStatic( vTickerTask, "ticker", configMINIMAL_STACK_SIZE, NULL,
                                     tskIDLE_PRIORITY + 2, xTickerStack, &xTickerTcb ) != NULL );
    configASSERT( xTaskCreateStatic( vWorkerTask, "worker", configMINIMAL_STACK_SIZE, NULL,
                                     tskIDLE_PRIORITY + 1, xWorkerStack, &xWorkerTcb ) != NULL );

    vTaskStartScheduler();

    /* Reached only after vTaskEndScheduler(). */
    printf( "scheduler ended, exit status %d\n", iExitStatus );
    fflush( stdout );
    return iExitStatus;
}
