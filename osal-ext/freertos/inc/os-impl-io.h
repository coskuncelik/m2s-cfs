/*
 * os-impl-io.h — m2s-cfs OSAL FreeRTOS layer: host socket handles of the Linux simulator (step 12a, D41).
 *
 * OSAL's portable BSD socket code (os-impl-bsd-sockets.c) keeps each socket's host file descriptor here, one
 * entry per OSAL stream. Only built with OS_FREERTOS_HOST_SOCKETS (simulator, networking on); RAM file
 * system files keep their own handles (os-impl-filesys.c) and have fd = -1 here.
 */
#ifndef OS_IMPL_IO_H
#define OS_IMPL_IO_H

#include "osconfig.h"
#include "common_types.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct
{
    int  fd;         /* host socket, or -1 when the stream is not a socket */
    bool selectable; /* set by the portable socket code once the socket is non-blocking */
} OS_impl_file_internal_record_t;

extern OS_impl_file_internal_record_t OS_impl_filehandle_table[OS_MAX_NUM_OPEN_FILES];

#endif /* OS_IMPL_IO_H */
