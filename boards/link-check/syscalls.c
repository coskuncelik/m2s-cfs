/*
 * syscalls.c — m2s-cfs newlib system stubs for Cortex-M3 boards without an OS underneath.
 *
 * No heap. The allocator entry points of newlib (malloc, free, calloc, realloc and their reentrant _r
 * forms) are defined here as traps: they record the call and halt. Because these definitions come first
 * in the link group, newlib's allocator objects are never linked; newlib-nano's printf core still
 * references _malloc_r/_free_r/_realloc_r (only used for growing strings, e.g. asprintf), and those
 * references land here. _sbrk is a trap too, for any other path that asks for heap memory.
 * The other stubs exist only if the C library references them; console output goes through OSAL
 * (OS_printf -> BSP -> board). stdout is unbuffered (set by the BSP), so stdio never allocates a buffer.
 */
#include <errno.h>
#include <reent.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#include "m2s_board.h"

#undef errno
extern int errno;

/* Set when something asked for heap memory; a debugger sees the requested size. */
volatile ptrdiff_t M2S_SbrkTrapIncrement;

void *_sbrk(ptrdiff_t incr)
{
    M2S_SbrkTrapIncrement = incr;
    __asm volatile("cpsid i");
    for (;;)
    {
        /* Dynamic allocation is not allowed on this target. */
    }
    return (void *)-1;
}

/* Which allocator entry point trapped (1 malloc, 2 free, 3 calloc, 4 realloc) and the requested size. */
volatile uint32_t M2S_AllocTrapWhich;
volatile size_t   M2S_AllocTrapSize;

static void M2S_AllocTrap(uint32_t which, size_t size) __attribute__((noreturn));
static void M2S_AllocTrap(uint32_t which, size_t size)
{
    M2S_AllocTrapWhich = which;
    M2S_AllocTrapSize  = size;
    __asm volatile("cpsid i");
    for (;;)
    {
        /* Dynamic allocation is not allowed on this target. */
    }
}

void *_malloc_r(struct _reent *r, size_t size)
{
    (void)r;
    M2S_AllocTrap(1U, size);
}

void _free_r(struct _reent *r, void *ptr)
{
    (void)r;
    (void)ptr;
    M2S_AllocTrap(2U, 0U);
}

void *_calloc_r(struct _reent *r, size_t n, size_t size)
{
    (void)r;
    M2S_AllocTrap(3U, n * size);
}

void *_realloc_r(struct _reent *r, void *ptr, size_t size)
{
    (void)r;
    (void)ptr;
    M2S_AllocTrap(4U, size);
}

void *malloc(size_t size)
{
    M2S_AllocTrap(1U, size);
}

void free(void *ptr)
{
    (void)ptr;
    M2S_AllocTrap(2U, 0U);
}

void *calloc(size_t n, size_t size)
{
    M2S_AllocTrap(3U, n * size);
}

void *realloc(void *ptr, size_t size)
{
    (void)ptr;
    M2S_AllocTrap(4U, size);
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

void _exit(int status)
{
    (void)status;
    __asm volatile("cpsid i");
    for (;;)
    {
    }
}
