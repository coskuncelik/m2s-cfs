/*
 * cfe_psp_start.c — m2s-cfs freertos-posix-sim PSP: start-up.
 *
 * The OSAL BSP (osal-ext/freertos-posix-sim) starts the FreeRTOS scheduler and calls
 * OS_Application_Startup() and then OS_Application_Run() from its static BSP_MAIN task.
 * Start-up: OSAL init -> reserved memory map -> /cf fixed map -> embedded /cf image unpacked (D5, D29)
 * -> PSP modules -> cFE (CFE_ES_Main). Every start is a power-on reset (static memory does not survive
 * the host process).
 */
#include <stdio.h>
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
 * Embedded /cf image (D5, D29): a POSIX ustar archive built by the mission scripts from the startup script
 * and any table files, embedded with <cpu>_EMBED_FILELIST "CFE_PSP_CF_IMAGE,cf.tar". Every mission using
 * this PSP must embed it: a missing image is a link error, not a silent start without apps.
 */
extern const char          CFE_PSP_CF_IMAGE_DATA[];
extern const unsigned long CFE_PSP_CF_IMAGE_SIZE;

#define CFE_PSP_TAR_BLOCK 512U

/* Parses an octal tar header field; returns false on a non-octal character. */
static bool CFE_PSP_TarOctal(const char *field, size_t len, unsigned long *value)
{
    size_t i;

    *value = 0;
    for (i = 0; i < len && field[i] != '\0' && field[i] != ' '; ++i)
    {
        if (field[i] < '0' || field[i] > '7')
        {
            return false;
        }
        *value = (*value << 3) + (unsigned long)(field[i] - '0');
    }
    return true;
}

/* Length of a tar header string field (not necessarily NUL-terminated). */
static size_t CFE_PSP_TarFieldLen(const char *field, size_t max)
{
    size_t n = 0;

    while (n < max && field[n] != '\0')
    {
        ++n;
    }
    return n;
}

/*
 * Builds "/cf/<prefix>/<name>" from a header. Returns 1 for a usable path, 0 for the archive root ("." or "./",
 * which is /cf itself and is skipped), -1 for an absolute path, "..", or a name that does not fit.
 */
static int CFE_PSP_TarPath(const char *hdr, char *path, size_t size)
{
    char        name[155 + 1 + 100 + 1]; /* ustar prefix + '/' + name + NUL */
    const char *rel;
    size_t      pos = 0;
    size_t      n;

    n = CFE_PSP_TarFieldLen(&hdr[345], 155); /* ustar prefix */
    if (n > 0)
    {
        memcpy(name, &hdr[345], n);
        pos         = n;
        name[pos++] = '/';
    }
    n = CFE_PSP_TarFieldLen(hdr, 100);
    memcpy(&name[pos], hdr, n);
    pos += n;
    name[pos] = '\0';

    while (pos > 0 && name[pos - 1] == '/') /* directories end in '/' */
    {
        name[--pos] = '\0';
    }
    rel = name;
    while (strncmp(rel, "./", 2) == 0)
    {
        rel += 2;
    }
    if (rel[0] == '\0' || strcmp(rel, ".") == 0)
    {
        return 0;
    }
    if (rel[0] == '/' || strstr(rel, "..") != NULL)
    {
        return -1;
    }
    n = (size_t)snprintf(path, size, "/cf/%s", rel);
    return (n < size) ? 1 : -1;
}

static int32 CFE_PSP_WriteCfFile(const char *path, const char *data, unsigned long size)
{
    osal_id_t fd;
    int32     status;

    status = OS_OpenCreate(&fd, path, OS_FILE_FLAG_CREATE | OS_FILE_FLAG_TRUNCATE, OS_WRITE_ONLY);
    if (status != OS_SUCCESS)
    {
        return status;
    }
    status = (size > 0) ? OS_write(fd, data, (size_t)size) : 0;
    (void)OS_close(fd);
    return (status == (int32)size) ? OS_SUCCESS : OS_ERROR;
}

/* Unpacks the embedded image into /cf: regular files and directories only. Reports and stops on any error. */
static void CFE_PSP_InstallCfImage(void)
{
    const char *  image = CFE_PSP_CF_IMAGE_DATA;
    unsigned long total = CFE_PSP_CF_IMAGE_SIZE;
    unsigned long off   = 0;
    unsigned long size;
    unsigned      files = 0;
    unsigned      dirs  = 0;
    char          path[OS_MAX_PATH_LEN];
    int           path_ok;
    int32         status;

    while (off + CFE_PSP_TAR_BLOCK <= total)
    {
        const char *hdr = &image[off];

        if (hdr[0] == '\0') /* end of archive (zero block) */
        {
            OS_printf("CFE_PSP: /cf image installed: %u file(s), %u director(ies)\n", files, dirs);
            return;
        }
        path_ok = CFE_PSP_TarPath(hdr, path, sizeof(path));
        if (memcmp(&hdr[257], "ustar", 5) != 0 || !CFE_PSP_TarOctal(&hdr[124], 12, &size) || path_ok < 0)
        {
            OS_printf("CFE_PSP: /cf image: bad entry at offset %lu\n", off);
            return;
        }
        off += CFE_PSP_TAR_BLOCK;
        if (size > total - off)
        {
            OS_printf("CFE_PSP: /cf image: %s is truncated\n", path);
            return;
        }

        switch ((path_ok == 0) ? 'R' : hdr[156]) /* 'R': archive root, nothing to do */
        {
            case 'R':
                break;
            case '0':
            case '\0':
                status = CFE_PSP_WriteCfFile(path, &image[off], size);
                if (status != OS_SUCCESS)
                {
                    OS_printf("CFE_PSP: cannot write %s: %d\n", path, (int)status);
                    return;
                }
                ++files;
                break;
            case '5':
                status = OS_mkdir(path, 0);
                if (status != OS_SUCCESS)
                {
                    OS_printf("CFE_PSP: cannot create directory %s: %d\n", path, (int)status);
                    return;
                }
                ++dirs;
                break;
            default:
                OS_printf("CFE_PSP: /cf image: %s has unsupported type '%c', skipped\n", path, hdr[156]);
                break;
        }
        off += (size + CFE_PSP_TAR_BLOCK - 1U) / CFE_PSP_TAR_BLOCK * CFE_PSP_TAR_BLOCK;
    }
    OS_printf("CFE_PSP: /cf image: no end-of-archive block\n");
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
        CFE_PSP_InstallCfImage();
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
