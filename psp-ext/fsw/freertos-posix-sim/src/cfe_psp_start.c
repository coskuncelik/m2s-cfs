/*
 * cfe_psp_start.c — m2s-cfs freertos-posix-sim PSP: start-up.
 *
 * The OSAL BSP (osal-ext/freertos-posix-sim) starts the FreeRTOS scheduler and calls
 * OS_Application_Startup() and then OS_Application_Run() from its static BSP_MAIN task.
 * Start-up: OSAL init -> reserved memory map -> /cf fixed map -> embedded startup script into /cf (D5)
 * -> PSP modules -> cFE (CFE_ES_Main). Every start is a power-on reset (static memory does not survive
 * the host process).
 */
#include <string.h>

#include "common_types.h"
#include "osapi.h"
#include "cfe_psp.h"
#include "cfe_psp_config.h"
#include "cfe_psp_memory.h"
#include "cfe_psp_module.h"
#include "target_config.h"

#define CFE_PSP_MAIN_FUNCTION       (*GLOBAL_CONFIGDATA.CfeConfig->SystemMain)
#define CFE_PSP_NONVOL_STARTUP_FILE (GLOBAL_CONFIGDATA.CfeConfig->NonvolStartupFile)

/*
 * Embedded startup script (targets.cmake: <cpu>_EMBED_FILELIST "CFE_ES_STARTUP_SCR,cfe_es_startup.scr").
 * Every mission using this PSP must embed it (D5): a missing script is a link error, not a silent start
 * without apps. (Weak references would not pull the data out of cFE's static embed library.)
 */
extern const char          CFE_ES_STARTUP_SCR_DATA[];
extern const unsigned long CFE_ES_STARTUP_SCR_SIZE;

/* Copies the embedded startup script to /cf. A failure is reported; cFE then starts without apps. */
static void CFE_PSP_InstallStartupScript(void)
{
    osal_id_t fd;
    int32     status;

    status = OS_OpenCreate(&fd, CFE_PSP_FREERTOS_STARTUP_SCRIPT, OS_FILE_FLAG_CREATE | OS_FILE_FLAG_TRUNCATE,
                           OS_WRITE_ONLY);
    if (status != OS_SUCCESS)
    {
        OS_printf("CFE_PSP: cannot create %s: %d\n", CFE_PSP_FREERTOS_STARTUP_SCRIPT, (int)status);
        return;
    }

    status = OS_write(fd, CFE_ES_STARTUP_SCR_DATA, (size_t)CFE_ES_STARTUP_SCR_SIZE);
    (void)OS_close(fd);

    if (status != (int32)CFE_ES_STARTUP_SCR_SIZE)
    {
        OS_printf("CFE_PSP: cannot write %s: %d\n", CFE_PSP_FREERTOS_STARTUP_SCRIPT, (int)status);
        return;
    }

    OS_printf("CFE_PSP: embedded startup script installed as %s (%lu bytes)\n", CFE_PSP_FREERTOS_STARTUP_SCRIPT,
              (unsigned long)CFE_ES_STARTUP_SCR_SIZE);
}

void CFE_PSP_Main(void)
{
    osal_id_t fs_id;
    int32     status;

    status = OS_API_Init();
    if (status != OS_SUCCESS)
    {
        /* OS_printf may not work yet; the BSP console is not needed for this path. */
        CFE_PSP_Panic(status);
    }

    CFE_PSP_SetupReservedMemoryMap();

    status = OS_FileSysAddFixedMap(&fs_id, CFE_PSP_FREERTOS_CF_LOCAL_PATH, "/cf");
    if (status != OS_SUCCESS)
    {
        OS_printf("CFE_PSP: OS_FileSysAddFixedMap() failure: %d\n", (int)status);
    }
    else
    {
        CFE_PSP_InstallStartupScript();
    }

    CFE_PSP_ModuleInit();

    CFE_PSP_InitProcessorReservedMemory(CFE_PSP_RST_TYPE_POWERON);

    /* Returns when cFE start-up is complete; cFE then runs in its own tasks. */
    CFE_PSP_MAIN_FUNCTION(CFE_PSP_RST_TYPE_POWERON, CFE_PSP_RST_SUBTYPE_POWER_CYCLE, 1, CFE_PSP_NONVOL_STARTUP_FILE);
}

void OS_Application_Startup(void)
{
    CFE_PSP_Main();
}

/* The BSP task has nothing more to do: cFE runs in its own tasks. Sleep forever. */
void OS_Application_Run(void)
{
    for (;;)
    {
        (void)OS_TaskDelay(1000);
    }
}
