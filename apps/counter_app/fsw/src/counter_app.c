/*
 * counter_app.c — m2s-cfs counter_app.
 *
 * A cFE application: registers with cFE, prints "counter_app initialized", then prints an incrementing
 * counter once per period (1 s by default) until cFE asks it to stop.
 *
 * Tasks: an OSAL timer gives a binary semaphore every period; a child task ("worker") takes it, increments the
 * counter under a mutex and prints it. The main task serves the command pipe (commands, housekeeping
 * requests) and the parameter table; SET_PERIOD and a table update reprogram the timer. The counter and the
 * period are shared by both tasks under the mutex. Output goes through OS_printf
 * (OSAL console -> BSP -> terminal or UART), so the same source runs on every target (D28).
 * Uses only the cFE and OSAL public APIs: no C library I/O, no platform code, no dynamic memory.
 *
 * Period: the OSAL timer's time base wakes at absolute tick deadlines, so the period does not drift (a
 * plain OS_TaskDelay loop drifts by up to a tick per period). The period comes from the parameter table
 * (default 1000 ms) and can be changed by command within the table's limits (counter_app_cmds.c).
 *
 * Events: EVS events for start-up, the file test and the library call (counter_app_events.h); EVS port 1
 * prints them on the console. The OS_printf lines stay, so the counter timing can be checked on the console.
 *
 * File test: once at start-up, before the 1 s schedule starts, the OSAL file API is exercised on
 * cFE's RAM disk (/ram): directory, write, size, read back, append, listing, rename, remove; then the startup
 * script is read from /cf. Static buffers only (the Cortex-M3 task stack is 4 KiB).
 *
 * Library: NASA's sample_lib, loaded by cFE before this app (CFE_LIB line in the startup
 * script); SAMPLE_LIB_Function is called once at start-up.
 */
#include <string.h>

#include "counter_app.h"
#include "counter_app_events.h"
#include "sample_lib.h"

COUNTER_APP_Data_t COUNTER_APP_Data;

#define COUNTER_APP_TEST_DIR      "/ram/counter"
#define COUNTER_APP_TEST_FILE     COUNTER_APP_TEST_DIR "/test.dat"
#define COUNTER_APP_TEST_FILE2    COUNTER_APP_TEST_DIR "/test2.dat"
#define COUNTER_APP_TEST_NAME     "test.dat"
#define COUNTER_APP_TEST_BYTES    256
#define COUNTER_APP_STARTUP_FILE  "/cf/cfe_es_startup.scr"
#define COUNTER_APP_STARTUP_BYTES 1024

static uint8 COUNTER_APP_WriteBuf[COUNTER_APP_TEST_BYTES];
static uint8 COUNTER_APP_ReadBuf[COUNTER_APP_TEST_BYTES];
static char  COUNTER_APP_StartupBuf[COUNTER_APP_STARTUP_BYTES + 1];

/* Reports a failed file test step (error event and console line); returns the step number. */
static int32 COUNTER_APP_FileStepFailed(int32 step, const char *what, int32 status)
{
    CFE_EVS_SendEvent(COUNTER_APP_FILE_TEST_ERR_EID, CFE_EVS_EventType_ERROR,
                      "File test error at step %ld (%s), status %ld", (long)step, what, (long)status);
    OS_printf("counter_app file test: FAIL step %ld\n", (long)step);
    return step;
}

/* Writes nbytes of the pattern buffer to an open file; returns OS_SUCCESS or the failing status. */
static int32 COUNTER_APP_WriteAll(osal_id_t fd, size_t nbytes)
{
    int32 n = OS_write(fd, COUNTER_APP_WriteBuf, nbytes);
    return (n == (int32)nbytes) ? OS_SUCCESS : (n < 0 ? n : OS_ERROR);
}

/* Returns OS_SUCCESS if path exists with the expected size, else the failing status. */
static int32 COUNTER_APP_CheckSize(const char *path, size_t expected)
{
    os_fstat_t st;
    int32      status = OS_stat(path, &st);
    if (status == OS_SUCCESS && OS_FILESTAT_SIZE(st) != expected)
    {
        status = OS_ERROR;
    }
    return status;
}

/*
 * The start-up file test. Returns 0 if every step passed, else the number of the first failing step (it has
 * already been reported). Open files and directories are always closed.
 */
static int32 COUNTER_APP_FileTest(void)
{
    osal_id_t   fd = OS_OBJECT_ID_UNDEFINED;
    os_dirent_t dirent;
    int32       status;
    int32       n;
    uint32      i;
    uint32      entries = 0;
    uint32      matches = 0;

    for (i = 0; i < COUNTER_APP_TEST_BYTES; ++i)
    {
        COUNTER_APP_WriteBuf[i] = (uint8)(i * 7U + 3U);
    }

    /* 1. Directory on the RAM disk. */
    status = OS_mkdir(COUNTER_APP_TEST_DIR, 0);
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(1, "mkdir " COUNTER_APP_TEST_DIR, status);
    }

    /* 2. Create and write the pattern. */
    status = OS_OpenCreate(&fd, COUNTER_APP_TEST_FILE, OS_FILE_FLAG_CREATE | OS_FILE_FLAG_TRUNCATE, OS_WRITE_ONLY);
    if (status == OS_SUCCESS)
    {
        status = COUNTER_APP_WriteAll(fd, COUNTER_APP_TEST_BYTES);
        (void)OS_close(fd);
    }
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(2, "create and write", status);
    }

    /* 3. Size. */
    status = COUNTER_APP_CheckSize(COUNTER_APP_TEST_FILE, COUNTER_APP_TEST_BYTES);
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(3, "size after write", status);
    }

    /* 4. Read back and compare. */
    memset(COUNTER_APP_ReadBuf, 0, sizeof(COUNTER_APP_ReadBuf));
    status = OS_OpenCreate(&fd, COUNTER_APP_TEST_FILE, OS_FILE_FLAG_NONE, OS_READ_ONLY);
    if (status == OS_SUCCESS)
    {
        n = OS_read(fd, COUNTER_APP_ReadBuf, sizeof(COUNTER_APP_ReadBuf));
        (void)OS_close(fd);
        if (n != COUNTER_APP_TEST_BYTES || memcmp(COUNTER_APP_ReadBuf, COUNTER_APP_WriteBuf, COUNTER_APP_TEST_BYTES) != 0)
        {
            status = (n < 0) ? n : OS_ERROR;
        }
    }
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(4, "read back and compare", status);
    }

    /* 5. Append (seek to the end, write again) and check the new size. */
    status = OS_OpenCreate(&fd, COUNTER_APP_TEST_FILE, OS_FILE_FLAG_NONE, OS_WRITE_ONLY);
    if (status == OS_SUCCESS)
    {
        n      = OS_lseek(fd, 0, OS_SEEK_END);
        status = (n == COUNTER_APP_TEST_BYTES) ? COUNTER_APP_WriteAll(fd, COUNTER_APP_TEST_BYTES) : (n < 0 ? n : OS_ERROR);
        (void)OS_close(fd);
    }
    if (status == OS_SUCCESS)
    {
        status = COUNTER_APP_CheckSize(COUNTER_APP_TEST_FILE, 2 * COUNTER_APP_TEST_BYTES);
    }
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(5, "append", status);
    }

    /* 6. Directory listing: exactly the test file. */
    status = OS_DirectoryOpen(&fd, COUNTER_APP_TEST_DIR);
    if (status == OS_SUCCESS)
    {
        while (OS_DirectoryRead(fd, &dirent) == OS_SUCCESS)
        {
            ++entries;
            if (strcmp(OS_DIRENTRY_NAME(dirent), COUNTER_APP_TEST_NAME) == 0)
            {
                ++matches;
            }
        }
        (void)OS_DirectoryClose(fd);
        if (entries != 1 || matches != 1)
        {
            status = OS_ERROR;
        }
    }
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(6, "directory listing", status);
    }

    /* 7. Rename, remove, remove the directory; the file must be gone. */
    status = OS_rename(COUNTER_APP_TEST_FILE, COUNTER_APP_TEST_FILE2);
    if (status == OS_SUCCESS)
    {
        status = OS_remove(COUNTER_APP_TEST_FILE2);
    }
    if (status == OS_SUCCESS)
    {
        status = OS_rmdir(COUNTER_APP_TEST_DIR);
    }
    if (status == OS_SUCCESS && COUNTER_APP_CheckSize(COUNTER_APP_TEST_FILE2, 2 * COUNTER_APP_TEST_BYTES) == OS_SUCCESS)
    {
        status = OS_ERROR;
    }
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(7, "rename and remove", status);
    }

    /* 8. Read the startup script from /cf; it must name this app. */
    memset(COUNTER_APP_StartupBuf, 0, sizeof(COUNTER_APP_StartupBuf));
    status = OS_OpenCreate(&fd, COUNTER_APP_STARTUP_FILE, OS_FILE_FLAG_NONE, OS_READ_ONLY);
    if (status == OS_SUCCESS)
    {
        n = OS_read(fd, COUNTER_APP_StartupBuf, COUNTER_APP_STARTUP_BYTES);
        (void)OS_close(fd);
        if (n <= 0 || strstr(COUNTER_APP_StartupBuf, "COUNTER_APP") == NULL)
        {
            status = (n < 0) ? n : OS_ERROR;
        }
    }
    if (status != OS_SUCCESS)
    {
        return COUNTER_APP_FileStepFailed(8, "read " COUNTER_APP_STARTUP_FILE, status);
    }
    CFE_EVS_SendEvent(COUNTER_APP_CF_READ_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "Read %s: %ld bytes, names COUNTER_APP", COUNTER_APP_STARTUP_FILE, (long)n);

    CFE_EVS_SendEvent(COUNTER_APP_FILE_TEST_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "File test passed on %s: wrote %d, read back, appended to %d bytes, listed, renamed, removed",
                      COUNTER_APP_TEST_DIR, COUNTER_APP_TEST_BYTES, 2 * COUNTER_APP_TEST_BYTES);
    OS_printf("counter_app file test: PASS\n");
    return 0;
}

/* Calls the library once; reports the result (event and console line). */
static void COUNTER_APP_LibraryCall(void)
{
    int32 status = SAMPLE_LIB_Function();

    COUNTER_APP_Data.LibCallStatus = status;
    if (status == CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(COUNTER_APP_LIB_CALL_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "sample_lib call returned 0x%08lX", (unsigned long)(uint32)status);
        OS_printf("counter_app sample_lib call: PASS\n");
    }
    else
    {
        CFE_EVS_SendEvent(COUNTER_APP_LIB_CALL_ERR_EID, CFE_EVS_EventType_ERROR,
                          "sample_lib call error, status 0x%08lX", (unsigned long)(uint32)status);
        OS_printf("counter_app sample_lib call: FAIL\n");
    }
}

/* Sets the period: shared value under the mutex, then the timer (if it exists yet). Main task only. */
void COUNTER_APP_SetPeriod(uint32 PeriodMs)
{
    int32 status;

    (void)OS_MutSemTake(COUNTER_APP_Data.DataMutex);
    COUNTER_APP_Data.PeriodMs = PeriodMs;
    (void)OS_MutSemGive(COUNTER_APP_Data.DataMutex);

    if (OS_ObjectIdDefined(COUNTER_APP_Data.TimerId))
    {
        status = OS_TimerSet(COUNTER_APP_Data.TimerId, PeriodMs * 1000U, PeriodMs * 1000U);
        if (status != OS_SUCCESS)
        {
            CFE_EVS_SendEvent(COUNTER_APP_WORKER_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Timer set error, period %lu ms, status %ld", (unsigned long)PeriodMs, (long)status);
        }
    }
}

/* Timer callback (runs in the time base's helper task): only wakes the worker. */
static void COUNTER_APP_TimerCallback(osal_id_t TimerId)
{
    (void)TimerId;
    (void)OS_BinSemGive(COUNTER_APP_Data.TickSem);
}

/* Worker task: one counter step per timer tick. */
static void COUNTER_APP_Worker(void)
{
    uint32 counter;
    int32  status;

    for (;;)
    {
        status = OS_BinSemTake(COUNTER_APP_Data.TickSem);
        if (status != OS_SUCCESS)
        {
            CFE_EVS_SendEvent(COUNTER_APP_WORKER_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Worker semaphore take error, status %ld; worker stops", (long)status);
            break;
        }
        (void)OS_MutSemTake(COUNTER_APP_Data.DataMutex);
        counter = ++COUNTER_APP_Data.Counter;
        (void)OS_MutSemGive(COUNTER_APP_Data.DataMutex);
        OS_printf("counter_app counter: %lu\n", (unsigned long)counter);
    }
    CFE_ES_ExitChildTask();
}

/* Mutex and tick semaphore; needed before the table sets the period. */
static int32 COUNTER_APP_CreateSync(void)
{
    int32 status = OS_MutSemCreate(&COUNTER_APP_Data.DataMutex, "COUNTER_DATA", 0);

    if (status == OS_SUCCESS)
    {
        status = OS_BinSemCreate(&COUNTER_APP_Data.TickSem, "COUNTER_TICK", OS_SEM_EMPTY, 0);
    }
    if (status != OS_SUCCESS)
    {
        CFE_EVS_SendEvent(COUNTER_APP_WORKER_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Mutex or semaphore create error, status %ld", (long)status);
    }
    return status;
}

/* Worker task, then the timer that drives it at the current period. */
static int32 COUNTER_APP_StartWorker(void)
{
    uint32 accuracy_us = 0;
    uint32 period_ms;
    int32  status;

    status = CFE_ES_CreateChildTask(&COUNTER_APP_Data.WorkerId, COUNTER_APP_WORKER_NAME, COUNTER_APP_Worker,
                                    CFE_ES_TASK_STACK_ALLOCATE, COUNTER_APP_WORKER_STACK,
                                    COUNTER_APP_WORKER_PRIORITY, 0);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(COUNTER_APP_WORKER_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Worker task create error, status 0x%08lX", (unsigned long)(uint32)status);
        return status;
    }

    status = OS_TimerCreate(&COUNTER_APP_Data.TimerId, "COUNTER_TIMER", &accuracy_us, COUNTER_APP_TimerCallback);
    if (status == OS_SUCCESS)
    {
        (void)OS_MutSemTake(COUNTER_APP_Data.DataMutex);
        period_ms = COUNTER_APP_Data.PeriodMs;
        (void)OS_MutSemGive(COUNTER_APP_Data.DataMutex);
        status = OS_TimerSet(COUNTER_APP_Data.TimerId, period_ms * 1000U, period_ms * 1000U);
    }
    if (status != OS_SUCCESS)
    {
        CFE_EVS_SendEvent(COUNTER_APP_WORKER_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Timer create or set error, status %ld", (long)status);
        return status;
    }
    CFE_EVS_SendEvent(COUNTER_APP_WORKER_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "Worker task and timer started: period %lu ms, timer accuracy %lu us", (unsigned long)period_ms,
                      (unsigned long)accuracy_us);
    return OS_SUCCESS;
}

void COUNTER_APP_Main(void)
{
    CFE_SB_Buffer_t *buf;
    CFE_Status_t     status;

    memset(&COUNTER_APP_Data, 0, sizeof(COUNTER_APP_Data));
    COUNTER_APP_Data.RunStatus = CFE_ES_RunStatus_APP_RUN;
    COUNTER_APP_Data.TimerId   = OS_OBJECT_ID_UNDEFINED;

    /* Register with Event Services (no event filters). */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        OS_printf("counter_app: CFE_EVS_Register failed, status 0x%08lX\n", (unsigned long)(uint32)status);
        COUNTER_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }
    else
    {
        OS_printf("counter_app initialized\n");
        CFE_EVS_SendEvent(COUNTER_APP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION, "counter_app initialized");
        /* Mutex, semaphore, pipe, table, worker and timer are needed for the counter: a failure stops the app. */
        if (COUNTER_APP_CreateSync() != OS_SUCCESS || COUNTER_APP_InitBusAndTable() != CFE_SUCCESS)
        {
            COUNTER_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
        /* A failed step is reported (error event, "file test: FAIL" line); the counter runs on regardless. */
        COUNTER_APP_Data.FileTestFailedStep = (uint8)COUNTER_APP_FileTest();
        COUNTER_APP_LibraryCall();
        if (COUNTER_APP_Data.RunStatus == CFE_ES_RunStatus_APP_RUN && COUNTER_APP_StartWorker() != OS_SUCCESS)
        {
            COUNTER_APP_Data.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /* Main task: commands and housekeeping requests as they arrive; table actions at least once a second. */
    while (CFE_ES_RunLoop(&COUNTER_APP_Data.RunStatus))
    {
        status = CFE_SB_ReceiveBuffer(&buf, COUNTER_APP_Data.CommandPipe, 1000);
        if (status == CFE_SUCCESS)
        {
            COUNTER_APP_ProcessMessage(buf);
        }
        else if (status != CFE_SB_TIME_OUT)
        {
            CFE_EVS_SendEvent(COUNTER_APP_SB_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Software Bus receive error, status 0x%08lX", (unsigned long)(uint32)status);
            (void)OS_TaskDelay(1000);
        }
        COUNTER_APP_ManageTable();
    }

    if (OS_ObjectIdDefined(COUNTER_APP_Data.TimerId))
    {
        (void)OS_TimerDelete(COUNTER_APP_Data.TimerId);
    }
    CFE_ES_ExitApp(COUNTER_APP_Data.RunStatus);
}
