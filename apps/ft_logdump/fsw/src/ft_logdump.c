/*
 * ft_logdump.c — m2s-cfs test-only app: prints cFE's functional test log to the console (step 05b, D29).
 *
 * cFE's test app writes the complete results to /cf/cfe_test.log; its console output goes through cFE
 * events, which EVS drops in bursts. On the simulator /cf is RAM, so this app copies the file to the
 * console once the test app has exited:
 *
 *   FT_LOGDUMP: BEGIN /cf/cfe_test.log
 *   FTLOG|<line> ...
 *   FT_LOGDUMP: END <n> bytes
 *
 * Each line of the file is printed as "FTLOG|<line>"; a line longer than FT_LOGDUMP_LINE is split into
 * "FTLOG+<part>" pieces (no line end) followed by the rest. Other tasks' console output is always whole lines
 * without these prefixes, so the script can rebuild the file exactly and compares its size with <n>.
 * Output is paced and this app runs below the OSAL console task's priority, so the console ring buffer does
 * not overflow.
 */
#include "cfe.h"

#define FT_LOGDUMP_TEST_APP     "CFE_TEST_APP"
#define FT_LOGDUMP_FILE         "/cf/cfe_test.log"
#define FT_LOGDUMP_CHUNK        128
#define FT_LOGDUMP_LINE         120 /* + prefix + newline stays well below OS_BUFFER_SIZE (172) */
#define FT_LOGDUMP_POLL_MS      200
#define FT_LOGDUMP_APPEAR_MS    60000 /* the test app must start within this time */
#define FT_LOGDUMP_PACE_MS      1     /* pause after each printed line */

static bool FT_LOGDUMP_TestAppRunning(void)
{
    CFE_ES_AppId_t id;

    return CFE_ES_GetAppIDByName(&id, FT_LOGDUMP_TEST_APP) == CFE_SUCCESS;
}

/* Prints one piece: complete = true for a piece that ended with a line end in the file. */
static void FT_LOGDUMP_Emit(char *line, uint32 *len, bool complete)
{
    line[*len] = '\0';
    OS_printf("FTLOG%c%s\n", complete ? '|' : '+', line);
    *len = 0;
    (void)OS_TaskDelay(FT_LOGDUMP_PACE_MS);
}

static void FT_LOGDUMP_Dump(void)
{
    char      chunk[FT_LOGDUMP_CHUNK];
    char      line[FT_LOGDUMP_LINE + 1];
    uint32    len = 0;
    osal_id_t fd;
    int32     status;
    int32     i;
    uint32    total = 0;

    status = OS_OpenCreate(&fd, FT_LOGDUMP_FILE, OS_FILE_FLAG_NONE, OS_READ_ONLY);
    if (status != OS_SUCCESS)
    {
        OS_printf("FT_LOGDUMP: cannot open %s: %ld\n", FT_LOGDUMP_FILE, (long)status);
        return;
    }

    OS_printf("FT_LOGDUMP: BEGIN %s\n", FT_LOGDUMP_FILE);
    for (;;)
    {
        status = OS_read(fd, chunk, FT_LOGDUMP_CHUNK);
        if (status <= 0)
        {
            break;
        }
        for (i = 0; i < status; ++i)
        {
            if (chunk[i] == '\n')
            {
                FT_LOGDUMP_Emit(line, &len, true);
            }
            else
            {
                line[len++] = chunk[i];
                if (len == FT_LOGDUMP_LINE)
                {
                    FT_LOGDUMP_Emit(line, &len, false);
                }
            }
        }
        total += (uint32)status;
    }
    if (len > 0) /* last line without a line end */
    {
        FT_LOGDUMP_Emit(line, &len, false);
    }
    (void)OS_close(fd);
    OS_printf("FT_LOGDUMP: END %lu bytes%s\n", (unsigned long)total, (status < 0) ? " (read error)" : "");
}

void FT_LOGDUMP_Main(void)
{
    uint32 run_status = CFE_ES_RunStatus_APP_RUN;
    uint32 waited     = 0;

    while (!FT_LOGDUMP_TestAppRunning() && waited < FT_LOGDUMP_APPEAR_MS)
    {
        (void)OS_TaskDelay(FT_LOGDUMP_POLL_MS);
        waited += FT_LOGDUMP_POLL_MS;
    }
    if (!FT_LOGDUMP_TestAppRunning())
    {
        OS_printf("FT_LOGDUMP: %s did not start\n", FT_LOGDUMP_TEST_APP);
    }
    else
    {
        while (FT_LOGDUMP_TestAppRunning())
        {
            (void)OS_TaskDelay(FT_LOGDUMP_POLL_MS);
        }
        (void)OS_TaskDelay(500); /* let the test app's last output reach the console first */
        FT_LOGDUMP_Dump();
    }

    while (CFE_ES_RunLoop(&run_status))
    {
        (void)OS_TaskDelay(1000);
    }
    CFE_ES_ExitApp(run_status);
}
