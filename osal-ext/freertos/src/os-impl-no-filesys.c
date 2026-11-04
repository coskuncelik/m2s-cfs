/*
 * os-impl-no-filesys.c — m2s-cfs OSAL FreeRTOS layer: file, directory and file system
 * functions for step 04a. All return OS_ERR_NOT_IMPLEMENTED.
 *
 * TEMPORARY: replaced by the static RAM file system in step 04b (docs/DECISIONS.md D19).
 */
#include "os-freertos.h"
#include "os-shared-file.h"
#include "os-shared-dir.h"
#include "os-shared-filesys.h"

int32 OS_FileSysStartVolume_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileSysStopVolume_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileSysFormatVolume_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileSysCheckVolume_Impl(const OS_object_token_t *token, bool repair)
{
    (void)token;
    (void)repair;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileSysStatVolume_Impl(const OS_object_token_t *token, OS_statvfs_t *result)
{
    (void)token;
    (void)result;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileSysMountVolume_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileSysUnmountVolume_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_GenericSeek_Impl(const OS_object_token_t *token, int32 offset, uint32 whence)
{
    (void)token;
    (void)offset;
    (void)whence;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_GenericRead_Impl(const OS_object_token_t *token, void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    (void)token;
    (void)buffer;
    (void)nbytes;
    (void)abs_timeout;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_GenericWrite_Impl(const OS_object_token_t *token, const void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    (void)token;
    (void)buffer;
    (void)nbytes;
    (void)abs_timeout;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_GenericClose_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileOpen_Impl(const OS_object_token_t *token, const char *local_path, int32 flags, int32 access_mode)
{
    (void)token;
    (void)local_path;
    (void)flags;
    (void)access_mode;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileStat_Impl(const char *local_path, os_fstat_t *filestat)
{
    (void)local_path;
    (void)filestat;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileRemove_Impl(const char *local_path)
{
    (void)local_path;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileRename_Impl(const char *old_path, const char *new_path)
{
    (void)old_path;
    (void)new_path;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_FileChmod_Impl(const char *local_path, uint32 access_mode)
{
    (void)local_path;
    (void)access_mode;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirCreate_Impl(const char *local_path, uint32 access)
{
    (void)local_path;
    (void)access;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirOpen_Impl(const OS_object_token_t *token, const char *local_path)
{
    (void)token;
    (void)local_path;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirClose_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirRead_Impl(const OS_object_token_t *token, os_dirent_t *dirent)
{
    (void)token;
    (void)dirent;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirRewind_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_DirRemove_Impl(const char *local_path)
{
    (void)local_path;
    return OS_ERR_NOT_IMPLEMENTED;
}
