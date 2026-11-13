/*
 * os-impl-host-sockets.c — m2s-cfs OSAL FreeRTOS layer: host sockets on the Linux simulator (step 12a, D41).
 *
 * Built only with OS_FREERTOS_HOST_SOCKETS (BSP freertos-posix-sim, OSAL networking on). OSAL's portable
 * os-impl-bsd-sockets.c (unchanged) opens, binds, connects, accepts, sends and receives with host sockets in
 * non-blocking mode and waits through OS_SelectSingle_Impl. This file provides:
 *   - the socket handle table (os-impl-io.h), fd = -1 for streams that are not sockets;
 *   - OS_SelectSingle_Impl / OS_SelectMultiple_Impl that POLL: a zero-timeout host select(), and between polls
 *     the task blocks in FreeRTOS for one tick. A task blocked in a host system call still counts as running
 *     for the FreeRTOS POSIX port and starves every lower-priority task (measured in step 12a phase 1); polling
 *     costs about 1 % of a host core per waiting task and adds at most one tick of latency;
 *   - read, write and close of socket streams, called from the OS_Generic*_Impl functions in
 *     os-impl-filesys.c. Writes use MSG_NOSIGNAL (no SIGPIPE when the peer has closed a TCP connection).
 * Host sockets use host kernel memory: simulator only, outside the embedded static-memory rules.
 */
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "os-freertos.h"
#include "os-impl-io.h"
#include "os-shared-idmap.h"
#include "os-shared-select.h"
#include "os-shared-clock.h"

OS_impl_file_internal_record_t OS_impl_filehandle_table[OS_MAX_NUM_OPEN_FILES];

int32 OS_FreeRTOS_HostSocketsInit(void)
{
    uint32 i;

    for (i = 0; i < OS_MAX_NUM_OPEN_FILES; ++i)
    {
        OS_impl_filehandle_table[i].fd         = -1;
        OS_impl_filehandle_table[i].selectable = false;
    }
    return OS_SUCCESS;
}

bool OS_FreeRTOS_IsHostSocket(const OS_object_token_t *token)
{
    return OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token)->fd >= 0;
}

/* true once abs_timeout has passed (OS_TIME_MAX never passes; a zero or negative time means "poll once"). */
static bool OS_FreeRTOS_TimeoutPassed(OS_time_t abs_timeout)
{
    OS_time_t now;

    if (OS_TimeEqual(abs_timeout, OS_TIME_MAX))
    {
        return false;
    }
    if (OS_TimeGetSign(abs_timeout) <= 0)
    {
        return true;
    }
    (void)OS_GetLocalTime_Impl(&now);
    return OS_TimeGetSign(OS_TimeSubtract(abs_timeout, now)) <= 0;
}

/*
 * Polls the sets until something is ready or abs_timeout passes. The caller's sets are restored before each
 * poll (select() overwrites them). Returns OS_SUCCESS (sets hold the ready descriptors), OS_ERROR_TIMEOUT or
 * OS_ERROR.
 */
static int32 OS_FreeRTOS_PollSelect(int maxfd, fd_set *rd_set, fd_set *wr_set, OS_time_t abs_timeout)
{
    fd_set         rd_want = *rd_set;
    fd_set         wr_want = *wr_set;
    struct timeval zero;
    int            rc;

    for (;;)
    {
        *rd_set      = rd_want;
        *wr_set      = wr_want;
        zero.tv_sec  = 0;
        zero.tv_usec = 0;
        rc           = select(maxfd + 1, rd_set, wr_set, NULL, &zero);
        if (rc > 0)
        {
            return OS_SUCCESS;
        }
        if (rc < 0 && errno != EINTR && errno != EAGAIN)
        {
            return OS_ERROR;
        }
        if (OS_FreeRTOS_TimeoutPassed(abs_timeout))
        {
            FD_ZERO(rd_set);
            FD_ZERO(wr_set);
            return OS_ERROR_TIMEOUT;
        }
        vTaskDelay(1);
    }
}

int32 OS_SelectSingle_Impl(const OS_object_token_t *token, uint32 *SelectFlags, OS_time_t abs_timeout)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    fd_set                          rd_set;
    fd_set                          wr_set;
    int32                           return_code;

    if (impl->fd < 0 || !impl->selectable || impl->fd >= FD_SETSIZE)
    {
        return OS_ERR_OPERATION_NOT_SUPPORTED; /* RAM file system files are not selectable */
    }
    if (*SelectFlags == 0)
    {
        return OS_SUCCESS;
    }

    FD_ZERO(&rd_set);
    FD_ZERO(&wr_set);
    if (*SelectFlags & OS_STREAM_STATE_READABLE)
    {
        FD_SET(impl->fd, &rd_set);
    }
    if (*SelectFlags & OS_STREAM_STATE_WRITABLE)
    {
        FD_SET(impl->fd, &wr_set);
    }
    return_code = OS_FreeRTOS_PollSelect(impl->fd, &rd_set, &wr_set, abs_timeout);
    if (return_code == OS_SUCCESS)
    {
        if (!FD_ISSET(impl->fd, &rd_set))
        {
            *SelectFlags &= ~OS_STREAM_STATE_READABLE;
        }
        if (!FD_ISSET(impl->fd, &wr_set))
        {
            *SelectFlags &= ~OS_STREAM_STATE_WRITABLE;
        }
    }
    else
    {
        *SelectFlags = 0;
    }
    return return_code;
}

/* OSAL id set -> host fd_set (selectable sockets only); maxfd = highest descriptor, -1 if none. */
static int32 OS_FreeRTOS_FdSetIn(int *maxfd, fd_set *os_set, const OS_FdSet *osal_set)
{
    size_t offset;
    size_t bit;
    int    fd;

    FD_ZERO(os_set);
    if (osal_set == NULL)
    {
        return OS_SUCCESS;
    }
    for (offset = 0; offset < sizeof(osal_set->object_ids); ++offset)
    {
        for (bit = 0; bit < 8; ++bit)
        {
            if ((osal_set->object_ids[offset] & (1U << bit)) == 0)
            {
                continue;
            }
            if (offset * 8 + bit >= OS_MAX_NUM_OPEN_FILES)
            {
                return OS_ERR_INVALID_ID;
            }
            fd = OS_impl_filehandle_table[offset * 8 + bit].fd;
            if (fd < 0 || !OS_impl_filehandle_table[offset * 8 + bit].selectable)
            {
                continue;
            }
            if (fd >= FD_SETSIZE)
            {
                return OS_ERR_OPERATION_NOT_SUPPORTED;
            }
            FD_SET(fd, os_set);
            if (fd > *maxfd)
            {
                *maxfd = fd;
            }
        }
    }
    return OS_SUCCESS;
}

/* Clears the OSAL bits whose descriptors are not set in the host set. */
static void OS_FreeRTOS_FdSetOut(const fd_set *os_set, OS_FdSet *osal_set)
{
    size_t offset;
    size_t bit;
    int    fd;

    if (osal_set == NULL)
    {
        return;
    }
    for (offset = 0; offset < sizeof(osal_set->object_ids); ++offset)
    {
        for (bit = 0; bit < 8; ++bit)
        {
            if ((osal_set->object_ids[offset] & (1U << bit)) == 0)
            {
                continue;
            }
            fd = (offset * 8 + bit < OS_MAX_NUM_OPEN_FILES) ? OS_impl_filehandle_table[offset * 8 + bit].fd : -1;
            if (fd < 0 || !FD_ISSET(fd, os_set))
            {
                osal_set->object_ids[offset] &= (uint8)~(1U << bit);
            }
        }
    }
}

int32 OS_SelectMultiple_Impl(OS_FdSet *ReadSet, OS_FdSet *WriteSet, OS_time_t abs_timeout)
{
    fd_set rd_set;
    fd_set wr_set;
    int    maxfd = -1;
    int32  return_code;

    return_code = OS_FreeRTOS_FdSetIn(&maxfd, &rd_set, ReadSet);
    if (return_code == OS_SUCCESS)
    {
        return_code = OS_FreeRTOS_FdSetIn(&maxfd, &wr_set, WriteSet);
    }
    if (return_code != OS_SUCCESS)
    {
        return return_code;
    }
    if (maxfd < 0)
    {
        return OS_ERR_OPERATION_NOT_SUPPORTED; /* nothing selectable in the sets */
    }
    return_code = OS_FreeRTOS_PollSelect(maxfd, &rd_set, &wr_set, abs_timeout);
    if (return_code == OS_SUCCESS)
    {
        OS_FreeRTOS_FdSetOut(&rd_set, ReadSet);
        OS_FreeRTOS_FdSetOut(&wr_set, WriteSet);
    }
    else
    {
        if (ReadSet != NULL)
        {
            memset(ReadSet, 0, sizeof(*ReadSet));
        }
        if (WriteSet != NULL)
        {
            memset(WriteSet, 0, sizeof(*WriteSet));
        }
    }
    return return_code;
}

/* Socket stream read: waits (polling) until readable or abs_timeout, then a non-blocking read. */
int32 OS_FreeRTOS_HostSocketRead(const OS_object_token_t *token, void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    OS_impl_file_internal_record_t *impl  = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    uint32                          flags = OS_STREAM_STATE_READABLE;
    int32                           rc;
    ssize_t                         n;

    if (impl->selectable)
    {
        rc = OS_SelectSingle_Impl(token, &flags, abs_timeout);
        if (rc != OS_SUCCESS)
        {
            return rc;
        }
    }
    do
    {
        n = recv(impl->fd, buffer, nbytes, MSG_DONTWAIT);
    } while (n < 0 && errno == EINTR);
    if (n < 0)
    {
        return (errno == EAGAIN || errno == EWOULDBLOCK) ? OS_ERROR_TIMEOUT : OS_ERROR;
    }
    return (int32)n;
}

/* Socket stream write: waits (polling) until writable or abs_timeout, then a non-blocking send (no SIGPIPE). */
int32 OS_FreeRTOS_HostSocketWrite(const OS_object_token_t *token, const void *buffer, size_t nbytes,
                                  OS_time_t abs_timeout)
{
    OS_impl_file_internal_record_t *impl  = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    uint32                          flags = OS_STREAM_STATE_WRITABLE;
    int32                           rc;
    ssize_t                         n;

    if (impl->selectable)
    {
        rc = OS_SelectSingle_Impl(token, &flags, abs_timeout);
        if (rc != OS_SUCCESS)
        {
            return rc;
        }
    }
    do
    {
        n = send(impl->fd, buffer, nbytes, MSG_DONTWAIT | MSG_NOSIGNAL);
    } while (n < 0 && errno == EINTR);
    if (n < 0)
    {
        return (errno == EAGAIN || errno == EWOULDBLOCK) ? OS_ERROR_TIMEOUT : OS_ERROR;
    }
    return (int32)n;
}

int32 OS_FreeRTOS_HostSocketClose(const OS_object_token_t *token)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    int                             rc   = close(impl->fd);

    impl->fd         = -1;
    impl->selectable = false;
    return (rc == 0 || errno == EINTR) ? OS_SUCCESS : OS_ERROR; /* after EINTR Linux has released the fd */
}
