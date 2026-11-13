/*
 * counter_app.c — m2s-cfs counter_app.
 *
 * A minimal cFE application: registers with cFE, prints "counter_app initialized", then prints an
 * incrementing counter once per second until cFE asks it to stop. Output goes through OS_printf
 * (OSAL console -> BSP -> terminal or UART), so the same source runs on every target (D28).
 * Uses only the cFE and OSAL public APIs: no C library I/O, no platform code, no dynamic memory.
 *
 * Period: the loop waits until the next fixed 1 s deadline (OS_GetLocalTime) rather than a plain
 * OS_TaskDelay(1000). OS_TaskDelay waits at least the time asked, up to one tick more, so plain delays
 * drift (about 1 ms per second measured on the simulator); deadlines keep the error within about a tick.
 *
 * Events (step 11a, D37): EVS events for start-up and the file test (counter_app_events.h); EVS port 1 prints
 * them on the console. The OS_printf lines stay: the run checks (scripts/lib/counter-check.sh) time them.
 *
 * File test (step 11a): once at start-up, before the 1 s schedule starts, the OSAL file API is exercised on
 * cFE's RAM disk (/ram): directory, write, size, read back, append, listing, rename, remove; then the startup
 * script is read from /cf. Static buffers only (the Cortex-M3 task stack is 4 KiB).
 */
#include <string.h>

#include "cfe.h"
#include "counter_app_events.h"

#define COUNTER_APP_PERIOD_MS 1000

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

/* Sleeps until *deadline + one period, then advances *deadline. Resynchronises if local time jumps. */
static void COUNTER_APP_WaitNextPeriod(OS_time_t *deadline)
{
    OS_time_t now;
    int64     remaining_ms;

    *deadline = OS_TimeAdd(*deadline, OS_TimeFromTotalMilliseconds(COUNTER_APP_PERIOD_MS));
    (void)OS_GetLocalTime(&now);
    remaining_ms = OS_TimeGetTotalMilliseconds(OS_TimeSubtract(*deadline, now));

    if (remaining_ms < 0 || remaining_ms > COUNTER_APP_PERIOD_MS)
    {
        /* Late by more than a period, or local time was set: start a new schedule from now. */
        *deadline    = OS_TimeAdd(now, OS_TimeFromTotalMilliseconds(COUNTER_APP_PERIOD_MS));
        remaining_ms = COUNTER_APP_PERIOD_MS;
    }
    if (remaining_ms > 0)
    {
        (void)OS_TaskDelay((uint32)remaining_ms);
    }
}

void COUNTER_APP_Main(void)
{
    uint32       run_status = CFE_ES_RunStatus_APP_RUN;
    uint32       counter    = 0;
    CFE_Status_t status;
    OS_time_t    deadline;

    /* Register with Event Services (no event filters). */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        OS_printf("counter_app: CFE_EVS_Register failed, status 0x%08lX\n", (unsigned long)status);
        run_status = CFE_ES_RunStatus_APP_ERROR;
    }
    else
    {
        OS_printf("counter_app initialized\n");
        CFE_EVS_SendEvent(COUNTER_APP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION, "counter_app initialized");
        /* A failed step is reported (error event, "file test: FAIL" line); the counter runs on regardless. */
        (void)COUNTER_APP_FileTest();
    }

    (void)OS_GetLocalTime(&deadline);

    while (CFE_ES_RunLoop(&run_status))
    {
        COUNTER_APP_WaitNextPeriod(&deadline);
        ++counter;
        OS_printf("counter_app counter: %lu\n", (unsigned long)counter);
    }

    CFE_ES_ExitApp(run_status);
}
