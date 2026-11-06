/*
 * syscalls.c — m2s-cfs generic Cortex-M3: newlib system stubs (no OS underneath), shared by all boards.
 *
 * No heap: _sbrk is a trap, for any path that asks newlib for heap memory. The allocator entry points
 * (malloc, free, calloc, realloc and their reentrant _r forms) are in a separate file, so a build links
 * exactly one of: alloc_traps.c (every flight image: each call traps) or test_heap.c (OSAL/cFE test images
 * only: ut_assert needs malloc). Either way they come first in the link group, so newlib's allocator
 * objects are never linked.
 * The other stubs exist only if the C library references them; console output goes through OSAL
 * (OS_printf -> BSP -> board). stdout is unbuffered (set by the BSP), so stdio never allocates a buffer.
 * Every trap prints one "M2S TRAP" line on the board console before halting (m2s_cm3_diag.h).
 */
#include <errno.h>
#include <reent.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#include "m2s_board.h"
#include "m2s_cm3_diag.h"

#undef errno
extern int errno;

/* Set when something asked for heap memory; a debugger sees the requested size. */
volatile ptrdiff_t M2S_SbrkTrapIncrement;

void *_sbrk(ptrdiff_t incr)
{
    M2S_SbrkTrapIncrement = incr;
    M2S_Diag_Write("\nM2S TRAP _sbrk increment ");
    M2S_Diag_WriteDec((uint32_t)incr);
    M2S_Diag_Write(" (dynamic allocation is not allowed)\n");
    M2S_Halt();
}

int _write(int fd, const char *buf, int len)
{
    if (fd == 1 || fd == 2)
    {
        M2S_Board_ConsoleWrite(buf, (size_t)len);
        return len;
    }
    errno = EBADF;
    return -1;
}

int _read(int fd, char *buf, int len)
{
    (void)fd;
    (void)buf;
    (void)len;
    errno = EBADF;
    return -1;
}

int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

int _lseek(int fd, int offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd)
{
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

/* exit() (e.g. OS_ApplicationExit, the end of an OSAL test) ends here, after newlib's exit handlers. */
void _exit(int status)
{
    M2S_Program_Exit(status);
}
