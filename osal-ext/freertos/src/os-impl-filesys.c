/*
 * os-impl-filesys.c — m2s-cfs OSAL FreeRTOS layer: files, directories and file systems on the
 * static RAM file system core (os-ramfs.c). Step 04b, docs/DECISIONS.md D19, D25.
 *
 * Volumes:
 *   - RAM disks (OS_mkfs / OS_initfs; every non-"fixed" file system is a RAM disk). The volume
 *     lives in the memory the caller passes, or, if none, in an area of a static pool. Its
 *     "system mount point" is "ramfs:<n>" (n = OSAL file system table index), so local paths
 *     look like "ramfs:3/dir/file".
 *   - One built-in root volume in static memory. Every other local path resolves there, e.g.
 *     the physical path of OS_FileSysAddFixedMap("./cf", "/cf") is the root directory "cf",
 *     created when the map is mounted.
 *
 * Nothing touches the host file system, so the simulator behaves like the target.
 * One static mutex serialises every call into the core. An open file or directory cannot be
 * removed, replaced by a rename, or have its volume stopped (OS_ERROR / OS_ERR_INCORRECT_OBJ_STATE).
 */
#include "os-freertos.h"
#include "os-ramfs.h"
#include "os-shared-file.h"
#include "os-shared-dir.h"
#include "os-shared-filesys.h"
#include "os-shared-idmap.h"
#include "os-shared-clock.h"

#if (RAMFS_NAME_LEN < OS_MAX_FILE_NAME)
#error "RAMFS_NAME_LEN must hold OS_MAX_FILE_NAME"
#endif

#define OS_FREERTOS_RAMFS_ROOT_VOL  OS_MAX_FILE_SYSTEMS /* index of the built-in root volume */
#define OS_FREERTOS_RAMFS_PREFIX    "ramfs:"
#define OS_FREERTOS_RAMFS_FILE_MODE (OS_FILESTAT_MODE_READ | OS_FILESTAT_MODE_WRITE)
#define OS_FREERTOS_RAMFS_DIR_MODE  (OS_FILESTAT_MODE_READ | OS_FILESTAT_MODE_WRITE | OS_FILESTAT_MODE_EXEC)
#define OS_FREERTOS_ALIGN8(x)       (((x) + 7U) & ~(size_t)7U)

typedef struct
{
    bool     started;
    bool     pooled;   /* memory is an area of OS_FreeRTOS_RamPool */
    uint8 *  mem;
    size_t   bytes;
    size_t   pool_off;
    RamFS_t *fs;       /* NULL until formatted / found valid */
} OS_FreeRTOS_RamVol_t;

typedef struct
{
    bool   in_use;
    uint32 vol;
    uint32 node;
    uint32 pos;
    int32  access;
} OS_FreeRTOS_FileHandle_t;

typedef struct
{
    bool   in_use;
    uint32 vol;
    uint32 node;
    uint32 cursor;
} OS_FreeRTOS_DirHandle_t;

static uint64                   OS_FreeRTOS_RamPool[OS_FREERTOS_RAMFS_POOL_BYTES / sizeof(uint64)];
static uint64                   OS_FreeRTOS_RamRoot[OS_FREERTOS_RAMFS_ROOT_BYTES / sizeof(uint64)];
static OS_FreeRTOS_RamVol_t     OS_FreeRTOS_RamVols[OS_MAX_FILE_SYSTEMS + 1];
static OS_FreeRTOS_FileHandle_t OS_FreeRTOS_FileHandles[OS_MAX_NUM_OPEN_FILES];
static OS_FreeRTOS_DirHandle_t  OS_FreeRTOS_DirHandles[OS_MAX_NUM_OPEN_DIRS];
static StaticSemaphore_t        OS_FreeRTOS_RamFsMutexBuf;
static SemaphoreHandle_t        OS_FreeRTOS_RamFsMutex;

/* ---- helpers ----------------------------------------------------------------------------- */

static void OS_FreeRTOS_RamFsLock(void)
{
    (void)xSemaphoreTake(OS_FreeRTOS_RamFsMutex, portMAX_DELAY);
}

static void OS_FreeRTOS_RamFsUnlock(void)
{
    (void)xSemaphoreGive(OS_FreeRTOS_RamFsMutex);
}

static int64 OS_FreeRTOS_RamFsNow(void)
{
    OS_time_t now = {0};

    (void)OS_GetLocalTime_Impl(&now);
    return now.ticks;
}

static uint32 OS_FreeRTOS_RamFsNodeCount(size_t bytes)
{
    size_t nodes = bytes / OS_FREERTOS_RAMFS_BYTES_PER_NODE;

    if (nodes < 8U)
    {
        nodes = 8U;
    }
    if (nodes > 4096U)
    {
        nodes = 4096U;
    }
    return (uint32)nodes;
}

/* Local path -> volume index and path inside that volume. Caller holds the lock. */
static int32 OS_FreeRTOS_RamFsResolve(const char *local_path, uint32 *vol, const char **rel)
{
    const size_t prefix_len = sizeof(OS_FREERTOS_RAMFS_PREFIX) - 1U;
    const char * p;
    uint32       index = 0;

    if (strncmp(local_path, OS_FREERTOS_RAMFS_PREFIX, prefix_len) != 0)
    {
        *vol = OS_FREERTOS_RAMFS_ROOT_VOL;
        *rel = local_path;
        return OS_SUCCESS;
    }

    p = local_path + prefix_len;
    if (*p < '0' || *p > '9')
    {
        return OS_FS_ERR_PATH_INVALID;
    }
    while (*p >= '0' && *p <= '9' && index < OS_MAX_FILE_SYSTEMS)
    {
        index = (index * 10U) + (uint32)(*p - '0');
        ++p;
    }
    if (index >= OS_MAX_FILE_SYSTEMS || (*p != '\0' && *p != '/') || OS_FreeRTOS_RamVols[index].fs == NULL)
    {
        return OS_FS_ERR_PATH_INVALID;
    }
    *vol = index;
    *rel = p;
    return OS_SUCCESS;
}

/* True if a file or directory handle refers to this node. Caller holds the lock. */
static bool OS_FreeRTOS_RamFsIsOpen(uint32 vol, uint32 node)
{
    uint32 i;

    for (i = 0; i < OS_MAX_NUM_OPEN_FILES; ++i)
    {
        if (OS_FreeRTOS_FileHandles[i].in_use && OS_FreeRTOS_FileHandles[i].vol == vol &&
            OS_FreeRTOS_FileHandles[i].node == node)
        {
            return true;
        }
    }
    for (i = 0; i < OS_MAX_NUM_OPEN_DIRS; ++i)
    {
        if (OS_FreeRTOS_DirHandles[i].in_use && OS_FreeRTOS_DirHandles[i].vol == vol &&
            OS_FreeRTOS_DirHandles[i].node == node)
        {
            return true;
        }
    }
    return false;
}

static bool OS_FreeRTOS_RamFsVolumeInUse(uint32 vol)
{
    uint32 i;

    for (i = 0; i < OS_MAX_NUM_OPEN_FILES; ++i)
    {
        if (OS_FreeRTOS_FileHandles[i].in_use && OS_FreeRTOS_FileHandles[i].vol == vol)
        {
            return true;
        }
    }
    for (i = 0; i < OS_MAX_NUM_OPEN_DIRS; ++i)
    {
        if (OS_FreeRTOS_DirHandles[i].in_use && OS_FreeRTOS_DirHandles[i].vol == vol)
        {
            return true;
        }
    }
    return false;
}

/* First-fit area of the static pool for a RAM disk with no caller memory. Caller holds the lock. */
static bool OS_FreeRTOS_RamPoolAlloc(size_t bytes, size_t *offset)
{
    size_t candidate = 0;
    bool   moved;
    uint32 i;

    bytes = OS_FREERTOS_ALIGN8(bytes);
    if (bytes == 0U || bytes > sizeof(OS_FreeRTOS_RamPool))
    {
        return false;
    }
    do
    {
        moved = false;
        for (i = 0; i < OS_MAX_FILE_SYSTEMS; ++i)
        {
            const OS_FreeRTOS_RamVol_t *v = &OS_FreeRTOS_RamVols[i];
            size_t                      end;

            if (!v->started || !v->pooled)
            {
                continue;
            }
            end = v->pool_off + OS_FREERTOS_ALIGN8(v->bytes);
            if (candidate < end && v->pool_off < candidate + bytes)
            {
                candidate = end;
                moved     = true;
            }
        }
    } while (moved && candidate + bytes <= sizeof(OS_FreeRTOS_RamPool));

    if (candidate + bytes > sizeof(OS_FreeRTOS_RamPool))
    {
        return false;
    }
    *offset = candidate;
    return true;
}

/* Create every directory along path in the root volume (for fixed-map mount points). */
static int32 OS_FreeRTOS_RamFsMakeDirs(const char *path)
{
    RamFS_t *    fs = OS_FreeRTOS_RamVols[OS_FREERTOS_RAMFS_ROOT_VOL].fs;
    char         partial[OS_MAX_LOCAL_PATH_LEN];
    size_t       len = strlen(path);
    size_t       i;
    uint32       node;
    RamFS_Info_t info;
    int32        rc;

    if (len >= sizeof(partial))
    {
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    }
    for (i = 1; i <= len; ++i)
    {
        if (path[i] != '/' && path[i] != '\0')
        {
            continue;
        }
        memcpy(partial, path, i);
        partial[i] = '\0';
        rc         = RamFS_Create(fs, partial, RAMFS_TYPE_DIR, OS_FREERTOS_RAMFS_DIR_MODE, &node);
        if (rc == RAMFS_ERR_EXISTS && RamFS_Lookup(fs, partial, &node) == RAMFS_OK &&
            RamFS_GetInfo(fs, node, &info) == RAMFS_OK && info.type == RAMFS_TYPE_DIR)
        {
            rc = RAMFS_OK;
        }
        if (rc != RAMFS_OK)
        {
            return OS_FS_ERR_DRIVE_NOT_CREATED;
        }
    }
    return OS_SUCCESS;
}

/* ---- init -------------------------------------------------------------------------------- */

int32 OS_FreeRTOS_FileSysAPI_Impl_Init(void)
{
    OS_FreeRTOS_RamFsMutex = xSemaphoreCreateMutexStatic(&OS_FreeRTOS_RamFsMutexBuf);

    memset(OS_FreeRTOS_RamVols, 0, sizeof(OS_FreeRTOS_RamVols));
    memset(OS_FreeRTOS_FileHandles, 0, sizeof(OS_FreeRTOS_FileHandles));
    memset(OS_FreeRTOS_DirHandles, 0, sizeof(OS_FreeRTOS_DirHandles));

    if (RamFS_Format(OS_FreeRTOS_RamRoot, sizeof(OS_FreeRTOS_RamRoot), OS_FREERTOS_RAMFS_ROOT_BLOCK_SIZE,
                     OS_FreeRTOS_RamFsNodeCount(sizeof(OS_FreeRTOS_RamRoot)), OS_FREERTOS_RAMFS_DIR_MODE) != RAMFS_OK)
    {
        return OS_ERROR;
    }
    OS_FreeRTOS_RamVols[OS_FREERTOS_RAMFS_ROOT_VOL].started = true;
    OS_FreeRTOS_RamVols[OS_FREERTOS_RAMFS_ROOT_VOL].mem     = (uint8 *)OS_FreeRTOS_RamRoot;
    OS_FreeRTOS_RamVols[OS_FREERTOS_RAMFS_ROOT_VOL].bytes   = sizeof(OS_FreeRTOS_RamRoot);
    OS_FreeRTOS_RamVols[OS_FREERTOS_RAMFS_ROOT_VOL].fs =
        RamFS_Attach(OS_FreeRTOS_RamRoot, sizeof(OS_FreeRTOS_RamRoot));

    return (OS_FreeRTOS_RamVols[OS_FREERTOS_RAMFS_ROOT_VOL].fs != NULL) ? OS_SUCCESS : OS_ERROR;
}

/* ---- file systems ------------------------------------------------------------------------ */

int32 OS_FileSysStartVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *filesys = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    uint32                        index   = (uint32)OS_ObjectIndexFromToken(token);
    OS_FreeRTOS_RamVol_t *        vol     = &OS_FreeRTOS_RamVols[index];
    uint64                        bytes   = (uint64)filesys->blocksize * (uint64)filesys->numblocks;
    size_t                        offset;
    int32                         return_code = OS_SUCCESS;
    char *                        mp;
    uint32                        digits;

    if (filesys->fstype == OS_FILESYS_TYPE_FS_BASED)
    {
        return OS_SUCCESS; /* the physical path is a directory of the root volume */
    }
    filesys->fstype = OS_FILESYS_TYPE_VOLATILE_DISK; /* every other volume is a RAM disk */

    if (bytes == 0U || bytes > (uint64)SIZE_MAX || (uint64)filesys->blocksize > UINT32_MAX)
    {
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    }

    OS_FreeRTOS_RamFsLock();
    memset(vol, 0, sizeof(*vol));
    vol->bytes = (size_t)bytes;
    if (filesys->address != NULL)
    {
        vol->mem = (uint8 *)filesys->address;
    }
    else if (OS_FreeRTOS_RamPoolAlloc(vol->bytes, &offset))
    {
        vol->mem      = (uint8 *)OS_FreeRTOS_RamPool + offset;
        vol->pooled   = true;
        vol->pool_off = offset;
    }
    else
    {
        OS_DEBUG("OS_FileSysStartVolume_Impl: RAM pool cannot hold %lu bytes\n", (unsigned long)bytes);
        return_code = OS_FS_ERR_DRIVE_NOT_CREATED;
    }
    if (return_code == OS_SUCCESS)
    {
        vol->started = true;
        vol->fs      = RamFS_Attach(vol->mem, vol->bytes); /* NULL until formatted (initfs keeps contents) */
    }
    OS_FreeRTOS_RamFsUnlock();

    if (return_code == OS_SUCCESS)
    {
        /* system mount point "ramfs:<index>" */
        mp = filesys->system_mountpt;
        memcpy(mp, OS_FREERTOS_RAMFS_PREFIX, sizeof(OS_FREERTOS_RAMFS_PREFIX) - 1U);
        mp += sizeof(OS_FREERTOS_RAMFS_PREFIX) - 1U;
        for (digits = 1; index / digits >= 10U; digits *= 10U)
        {
        }
        for (; digits > 0U; digits /= 10U)
        {
            *mp++ = (char)('0' + ((index / digits) % 10U));
        }
        *mp = '\0';
    }
    return return_code;
}

int32 OS_FileSysStopVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *filesys = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    uint32                        index   = (uint32)OS_ObjectIndexFromToken(token);
    int32                         return_code = OS_SUCCESS;

    if (filesys->fstype == OS_FILESYS_TYPE_FS_BASED)
    {
        return OS_SUCCESS;
    }
    OS_FreeRTOS_RamFsLock();
    if (OS_FreeRTOS_RamFsVolumeInUse(index))
    {
        return_code = OS_ERR_INCORRECT_OBJ_STATE;
    }
    else
    {
        memset(&OS_FreeRTOS_RamVols[index], 0, sizeof(OS_FreeRTOS_RamVols[index])); /* frees the pool area */
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileSysFormatVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *filesys = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    OS_FreeRTOS_RamVol_t *        vol     = &OS_FreeRTOS_RamVols[OS_ObjectIndexFromToken(token)];
    int32                         return_code = OS_SUCCESS;

    if (filesys->fstype == OS_FILESYS_TYPE_FS_BASED)
    {
        return OS_ERR_NOT_IMPLEMENTED;
    }
    OS_FreeRTOS_RamFsLock();
    vol->fs = NULL;
    if (!vol->started ||
        RamFS_Format(vol->mem, vol->bytes, (uint32)filesys->blocksize, OS_FreeRTOS_RamFsNodeCount(vol->bytes),
                     OS_FREERTOS_RAMFS_DIR_MODE) != RAMFS_OK)
    {
        return_code = OS_FS_ERR_DRIVE_NOT_CREATED;
    }
    else
    {
        vol->fs = RamFS_Attach(vol->mem, vol->bytes);
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileSysCheckVolume_Impl(const OS_object_token_t *token, bool repair)
{
    OS_filesys_internal_record_t *filesys = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    uint32                        index   = (uint32)OS_ObjectIndexFromToken(token);
    RamFS_t *                     fs;
    int32                         return_code;

    (void)repair; /* no repair: a damaged RAM disk is reformatted by its user */
    if (filesys->fstype == OS_FILESYS_TYPE_FS_BASED)
    {
        index = OS_FREERTOS_RAMFS_ROOT_VOL;
    }
    OS_FreeRTOS_RamFsLock();
    fs          = OS_FreeRTOS_RamVols[index].fs;
    return_code = (fs != NULL && RamFS_Check(fs) == RAMFS_OK) ? OS_SUCCESS : OS_ERROR;
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileSysStatVolume_Impl(const OS_object_token_t *token, OS_statvfs_t *result)
{
    OS_filesys_internal_record_t *filesys = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    uint32                        index   = (uint32)OS_ObjectIndexFromToken(token);
    uint32                        block_size;
    uint32                        total;
    uint32                        nfree;
    int32                         return_code = OS_ERROR;

    if (filesys->fstype == OS_FILESYS_TYPE_FS_BASED)
    {
        index = OS_FREERTOS_RAMFS_ROOT_VOL;
    }
    OS_FreeRTOS_RamFsLock();
    if (OS_FreeRTOS_RamVols[index].fs != NULL)
    {
        RamFS_Usage(OS_FreeRTOS_RamVols[index].fs, &block_size, &total, &nfree);
        result->block_size   = OSAL_SIZE_C(block_size);
        result->total_blocks = OSAL_BLOCKCOUNT_C(total);
        result->blocks_free  = OSAL_BLOCKCOUNT_C(nfree);
        return_code          = OS_SUCCESS;
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileSysMountVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *filesys = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    OS_FreeRTOS_RamVol_t *        vol     = &OS_FreeRTOS_RamVols[OS_ObjectIndexFromToken(token)];
    int32                         return_code;

    OS_FreeRTOS_RamFsLock();
    if (filesys->fstype == OS_FILESYS_TYPE_FS_BASED)
    {
        return_code = OS_FreeRTOS_RamFsMakeDirs(filesys->system_mountpt);
    }
    else
    {
        /* initfs on memory without a valid volume ends here */
        vol->fs     = vol->started ? RamFS_Attach(vol->mem, vol->bytes) : NULL;
        return_code = (vol->fs != NULL) ? OS_SUCCESS : OS_FS_ERR_DRIVE_NOT_CREATED;
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileSysUnmountVolume_Impl(const OS_object_token_t *token)
{
    (void)token; /* contents stay; the shared layer stops path translation */
    return OS_SUCCESS;
}

/* ---- open files -------------------------------------------------------------------------- */

int32 OS_FileOpen_Impl(const OS_object_token_t *token, const char *local_path, int32 flags, int32 access_mode)
{
    OS_FreeRTOS_FileHandle_t *h  = OS_OBJECT_TABLE_GET(OS_FreeRTOS_FileHandles, *token);
    RamFS_t *                 fs = NULL;
    const char *              rel;
    uint32                    vol  = 0;
    uint32                    node = 0;
    RamFS_Info_t              info = {0};
    int32                     rc;
    int32                     return_code;

    if (access_mode != OS_READ_ONLY && access_mode != OS_WRITE_ONLY && access_mode != OS_READ_WRITE)
    {
        return OS_ERROR;
    }

    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(local_path, &vol, &rel);
    if (return_code == OS_SUCCESS)
    {
        fs = OS_FreeRTOS_RamVols[vol].fs;
        rc = RamFS_Lookup(fs, rel, &node);
        if (rc == RAMFS_ERR_NOT_FOUND && (flags & OS_FILE_FLAG_CREATE) != 0)
        {
            rc = RamFS_Create(fs, rel, RAMFS_TYPE_FILE, OS_FREERTOS_RAMFS_FILE_MODE, &node);
            if (rc == RAMFS_OK)
            {
                (void)RamFS_SetTime(fs, node, OS_FreeRTOS_RamFsNow());
            }
        }
        if (rc != RAMFS_OK || RamFS_GetInfo(fs, node, &info) != RAMFS_OK || info.type != RAMFS_TYPE_FILE ||
            (access_mode != OS_WRITE_ONLY && (info.mode & OS_FILESTAT_MODE_READ) == 0) ||
            (access_mode != OS_READ_ONLY && (info.mode & OS_FILESTAT_MODE_WRITE) == 0))
        {
            return_code = OS_ERROR;
        }
    }
    if (return_code == OS_SUCCESS)
    {
        if ((flags & OS_FILE_FLAG_TRUNCATE) != 0 && access_mode != OS_READ_ONLY && info.size > 0U)
        {
            (void)RamFS_Truncate(fs, node);
            (void)RamFS_SetTime(fs, node, OS_FreeRTOS_RamFsNow());
        }
        h->in_use = true;
        h->vol    = vol;
        h->node   = node;
        h->pos    = 0;
        h->access = access_mode;
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_GenericSeek_Impl(const OS_object_token_t *token, int32 offset, uint32 whence)
{
    OS_FreeRTOS_FileHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_FileHandles, *token);
    RamFS_Info_t              info;
    int64                     base;
    int64                     target;
    int32                     return_code = OS_ERROR;

    OS_FreeRTOS_RamFsLock();
    if (h->in_use && RamFS_GetInfo(OS_FreeRTOS_RamVols[h->vol].fs, h->node, &info) == RAMFS_OK)
    {
        switch (whence)
        {
            case OS_SEEK_SET:
                base = 0;
                break;
            case OS_SEEK_CUR:
                base = h->pos;
                break;
            case OS_SEEK_END:
                base = info.size;
                break;
            default:
                base = -1;
                break;
        }
        target = base + offset;
        if (base >= 0 && target >= 0 && target <= INT32_MAX)
        {
            h->pos      = (uint32)target;
            return_code = (int32)target;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_GenericRead_Impl(const OS_object_token_t *token, void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    OS_FreeRTOS_FileHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_FileHandles, *token);
    int32                     rc;

    (void)abs_timeout; /* a RAM file never blocks */
    if (nbytes > (size_t)INT32_MAX)
    {
        nbytes = (size_t)INT32_MAX;
    }

    OS_FreeRTOS_RamFsLock();
    if (!h->in_use || h->access == OS_WRITE_ONLY)
    {
        rc = OS_ERROR;
    }
    else
    {
        rc = RamFS_Read(OS_FreeRTOS_RamVols[h->vol].fs, h->node, h->pos, buffer, (uint32)nbytes);
        if (rc >= 0)
        {
            h->pos += (uint32)rc;
        }
        else
        {
            rc = OS_ERROR;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return rc;
}

int32 OS_GenericWrite_Impl(const OS_object_token_t *token, const void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    OS_FreeRTOS_FileHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_FileHandles, *token);
    RamFS_t *                 fs;
    int32                     rc;

    (void)abs_timeout;
    if (nbytes > (size_t)INT32_MAX)
    {
        nbytes = (size_t)INT32_MAX;
    }

    OS_FreeRTOS_RamFsLock();
    if (!h->in_use || h->access == OS_READ_ONLY)
    {
        rc = OS_ERROR;
    }
    else
    {
        fs = OS_FreeRTOS_RamVols[h->vol].fs;
        rc = RamFS_Write(fs, h->node, h->pos, buffer, (uint32)nbytes);
        if (rc >= 0)
        {
            h->pos += (uint32)rc;
            if (rc > 0)
            {
                (void)RamFS_SetTime(fs, h->node, OS_FreeRTOS_RamFsNow());
            }
        }
        else
        {
            rc = OS_ERROR; /* volume full (nothing written) or position out of range */
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return rc;
}

int32 OS_GenericClose_Impl(const OS_object_token_t *token)
{
    OS_FreeRTOS_FileHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_FileHandles, *token);

    OS_FreeRTOS_RamFsLock();
    h->in_use = false;
    OS_FreeRTOS_RamFsUnlock();
    return OS_SUCCESS;
}

/* ---- files by name ----------------------------------------------------------------------- */

int32 OS_FileStat_Impl(const char *local_path, os_fstat_t *filestat)
{
    const char * rel;
    uint32       vol;
    uint32       node;
    RamFS_Info_t info;
    int32        return_code;

    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(local_path, &vol, &rel);
    if (return_code == OS_SUCCESS)
    {
        if (RamFS_Lookup(OS_FreeRTOS_RamVols[vol].fs, rel, &node) != RAMFS_OK ||
            RamFS_GetInfo(OS_FreeRTOS_RamVols[vol].fs, node, &info) != RAMFS_OK)
        {
            return_code = OS_ERROR;
        }
        else
        {
            memset(filestat, 0, sizeof(*filestat));
            filestat->FileModeBits   = info.mode | ((info.type == RAMFS_TYPE_DIR) ? OS_FILESTAT_MODE_DIR : 0U);
            filestat->FileSize       = info.size;
            filestat->FileTime.ticks = info.mtime;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileRemove_Impl(const char *local_path)
{
    const char *rel;
    uint32      vol;
    uint32      node;
    int32       return_code;

    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(local_path, &vol, &rel);
    if (return_code == OS_SUCCESS)
    {
        if (RamFS_Lookup(OS_FreeRTOS_RamVols[vol].fs, rel, &node) != RAMFS_OK ||
            OS_FreeRTOS_RamFsIsOpen(vol, node) ||
            RamFS_Remove(OS_FreeRTOS_RamVols[vol].fs, rel, RAMFS_TYPE_FILE) != RAMFS_OK)
        {
            return_code = OS_ERROR;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileRename_Impl(const char *old_path, const char *new_path)
{
    const char *old_rel;
    const char *new_rel;
    uint32      old_vol;
    uint32      new_vol;
    uint32      target;
    int32       return_code;

    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(old_path, &old_vol, &old_rel);
    if (return_code == OS_SUCCESS)
    {
        return_code = OS_FreeRTOS_RamFsResolve(new_path, &new_vol, &new_rel);
    }
    if (return_code == OS_SUCCESS)
    {
        /* Same volume only; an open file cannot be replaced. */
        if (old_vol != new_vol ||
            (RamFS_Lookup(OS_FreeRTOS_RamVols[new_vol].fs, new_rel, &target) == RAMFS_OK &&
             OS_FreeRTOS_RamFsIsOpen(new_vol, target)) ||
            RamFS_Rename(OS_FreeRTOS_RamVols[old_vol].fs, old_rel, new_rel) != RAMFS_OK)
        {
            return_code = OS_ERROR;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_FileChmod_Impl(const char *local_path, uint32 access_mode)
{
    const char * rel;
    uint32       vol;
    uint32       node;
    uint32       mode;
    RamFS_Info_t info;
    int32        return_code;

    switch (access_mode)
    {
        case OS_READ_ONLY:
            mode = OS_FILESTAT_MODE_READ;
            break;
        case OS_WRITE_ONLY:
            mode = OS_FILESTAT_MODE_WRITE;
            break;
        case OS_READ_WRITE:
            mode = OS_FILESTAT_MODE_READ | OS_FILESTAT_MODE_WRITE;
            break;
        default:
            return OS_ERROR;
    }

    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(local_path, &vol, &rel);
    if (return_code == OS_SUCCESS)
    {
        if (RamFS_Lookup(OS_FreeRTOS_RamVols[vol].fs, rel, &node) != RAMFS_OK ||
            RamFS_GetInfo(OS_FreeRTOS_RamVols[vol].fs, node, &info) != RAMFS_OK)
        {
            return_code = OS_ERROR;
        }
        else
        {
            /* the execute bit is kept */
            (void)RamFS_SetMode(OS_FreeRTOS_RamVols[vol].fs, node, mode | (info.mode & OS_FILESTAT_MODE_EXEC));
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

/* ---- directories ------------------------------------------------------------------------- */

int32 OS_DirCreate_Impl(const char *local_path, uint32 access)
{
    const char * rel;
    uint32       vol;
    uint32       node;
    RamFS_Info_t info;
    int32        rc;
    int32        return_code;

    (void)access;
    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(local_path, &vol, &rel);
    if (return_code == OS_SUCCESS)
    {
        rc = RamFS_Create(OS_FreeRTOS_RamVols[vol].fs, rel, RAMFS_TYPE_DIR, OS_FREERTOS_RAMFS_DIR_MODE, &node);
        if (rc == RAMFS_OK)
        {
            (void)RamFS_SetTime(OS_FreeRTOS_RamVols[vol].fs, node, OS_FreeRTOS_RamFsNow());
        }
        else if (rc == RAMFS_ERR_EXISTS && RamFS_Lookup(OS_FreeRTOS_RamVols[vol].fs, rel, &node) == RAMFS_OK &&
                 RamFS_GetInfo(OS_FreeRTOS_RamVols[vol].fs, node, &info) == RAMFS_OK && info.type == RAMFS_TYPE_DIR)
        {
            /* an existing directory is success, as in the POSIX implementation */
        }
        else
        {
            return_code = OS_ERROR;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_DirOpen_Impl(const OS_object_token_t *token, const char *local_path)
{
    OS_FreeRTOS_DirHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_DirHandles, *token);
    const char *             rel;
    uint32                   vol;
    uint32                   node;
    RamFS_Info_t             info;
    int32                    return_code;

    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(local_path, &vol, &rel);
    if (return_code == OS_SUCCESS)
    {
        if (RamFS_Lookup(OS_FreeRTOS_RamVols[vol].fs, rel, &node) != RAMFS_OK ||
            RamFS_GetInfo(OS_FreeRTOS_RamVols[vol].fs, node, &info) != RAMFS_OK || info.type != RAMFS_TYPE_DIR)
        {
            return_code = OS_ERROR;
        }
        else
        {
            h->in_use = true;
            h->vol    = vol;
            h->node   = node;
            h->cursor = 0;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_DirClose_Impl(const OS_object_token_t *token)
{
    OS_FreeRTOS_DirHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_DirHandles, *token);

    OS_FreeRTOS_RamFsLock();
    h->in_use = false;
    OS_FreeRTOS_RamFsUnlock();
    return OS_SUCCESS;
}

int32 OS_DirRead_Impl(const OS_object_token_t *token, os_dirent_t *dirent)
{
    OS_FreeRTOS_DirHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_DirHandles, *token);
    char                     name[RAMFS_NAME_LEN];
    int32                    return_code = OS_ERROR; /* also the "end of directory" result */

    OS_FreeRTOS_RamFsLock();
    if (h->in_use && RamFS_DirNext(OS_FreeRTOS_RamVols[h->vol].fs, h->node, &h->cursor, name) == RAMFS_OK)
    {
        strncpy(dirent->FileName, name, sizeof(dirent->FileName) - 1U);
        dirent->FileName[sizeof(dirent->FileName) - 1U] = '\0';
        return_code                                     = OS_SUCCESS;
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}

int32 OS_DirRewind_Impl(const OS_object_token_t *token)
{
    OS_FreeRTOS_DirHandle_t *h = OS_OBJECT_TABLE_GET(OS_FreeRTOS_DirHandles, *token);

    OS_FreeRTOS_RamFsLock();
    h->cursor = 0;
    OS_FreeRTOS_RamFsUnlock();
    return OS_SUCCESS;
}

int32 OS_DirRemove_Impl(const char *local_path)
{
    const char *rel;
    uint32      vol;
    uint32      node;
    int32       return_code;

    OS_FreeRTOS_RamFsLock();
    return_code = OS_FreeRTOS_RamFsResolve(local_path, &vol, &rel);
    if (return_code == OS_SUCCESS)
    {
        if (RamFS_Lookup(OS_FreeRTOS_RamVols[vol].fs, rel, &node) != RAMFS_OK ||
            OS_FreeRTOS_RamFsIsOpen(vol, node) ||
            RamFS_Remove(OS_FreeRTOS_RamVols[vol].fs, rel, RAMFS_TYPE_DIR) != RAMFS_OK)
        {
            return_code = OS_ERROR;
        }
    }
    OS_FreeRTOS_RamFsUnlock();
    return return_code;
}
