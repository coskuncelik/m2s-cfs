/*
 * alloc_traps.c — m2s-cfs generic Cortex-M3: the allocator entry points as traps (every flight image).
 *
 * malloc, free, calloc, realloc and newlib's reentrant _r forms record the call, print one "M2S TRAP" line
 * on the board console and halt. Because these definitions come first in the link group (the toolchain's
 * -u options), newlib's allocator objects are never linked; newlib-nano's printf core still references
 * _malloc_r/_free_r/_realloc_r (only used for growing strings, e.g. asprintf), and those references land
 * here. Test images link test_heap.c instead (M2S_CM3_TEST_HEAP), never both.
 */
#include <reent.h>
#include <stddef.h>
#include <stdint.h>

#include "m2s_cm3_diag.h"

/* Which allocator entry point trapped (1 malloc, 2 free, 3 calloc, 4 realloc) and the requested size. */
volatile uint32_t M2S_AllocTrapWhich;
volatile size_t   M2S_AllocTrapSize;

static void M2S_AllocTrap(uint32_t which, size_t size) __attribute__((noreturn));
static void M2S_AllocTrap(uint32_t which, size_t size)
{
    static const char *const names[] = {"?", "malloc", "free", "calloc", "realloc"};

    M2S_AllocTrapWhich = which;
    M2S_AllocTrapSize  = size;
    M2S_Diag_Write("\nM2S TRAP ");
    M2S_Diag_Write(names[(which < 5U) ? which : 0U]);
    M2S_Diag_Write(" size ");
    M2S_Diag_WriteDec((uint32_t)size);
    M2S_Diag_Write(" (dynamic allocation is not allowed)\n");
    M2S_Halt();
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
