/*
 * test_heap.c — m2s-cfs generic Cortex-M3: TEST-ONLY allocator for OSAL/cFE test images (D33).
 *
 * OSAL's test library ut_assert allocates its test registry and stub buffers with malloc (utlist.c,
 * utstubs.c). Test images (CMake option M2S_CM3_TEST_HEAP) link this file instead of alloc_traps.c; flight
 * images never do, and the scripts check that.
 *
 * One static pool, allocated forward only: free() does nothing and memory is never reused, so there is no
 * fragmentation and no allocator state to corrupt. Blocks are 8-byte aligned and carry their size (for
 * realloc). An empty pool prints one "M2S TRAP" line and halts. M2S_TestHeap_Report() prints the
 * high-water mark; the BSP calls it when the test program ends.
 */
#include <reent.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "m2s_cm3_diag.h"

#ifndef M2S_TEST_HEAP_BYTES
#define M2S_TEST_HEAP_BYTES (1024U * 1024U)
#endif

#define M2S_TEST_HEAP_ALIGN 8U
#define M2S_TEST_HEAP_HDR   M2S_TEST_HEAP_ALIGN /* block header: its size, padded to the alignment */

static uint64_t M2S_TestHeapPool[M2S_TEST_HEAP_BYTES / sizeof(uint64_t)];
static size_t   M2S_TestHeapUsed;

void M2S_TestHeap_Report(void);

static void *M2S_TestHeap_Alloc(size_t size)
{
    size_t   need = (size + M2S_TEST_HEAP_ALIGN - 1U) & ~(size_t)(M2S_TEST_HEAP_ALIGN - 1U);
    uint8_t *block;
    uint32_t primask;

    /* Tasks may allocate concurrently: check and bump with interrupts off, previous PRIMASK restored. */
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(primask)::"memory");
    if (need < size || need > sizeof(M2S_TestHeapPool) - M2S_TEST_HEAP_HDR ||
        M2S_TestHeapUsed > sizeof(M2S_TestHeapPool) - M2S_TEST_HEAP_HDR - need)
    {
        M2S_Diag_Write("\nM2S TRAP test heap exhausted: request ");
        M2S_Diag_WriteDec((uint32_t)size);
        M2S_Diag_Write(", used ");
        M2S_Diag_WriteDec((uint32_t)M2S_TestHeapUsed);
        M2S_Diag_Write(" of ");
        M2S_Diag_WriteDec((uint32_t)sizeof(M2S_TestHeapPool));
        M2S_Diag_Write("\n");
        M2S_Halt();
    }
    block = (uint8_t *)M2S_TestHeapPool + M2S_TestHeapUsed;
    M2S_TestHeapUsed += M2S_TEST_HEAP_HDR + need;
    __asm volatile("msr primask, %0" ::"r"(primask) : "memory");

    memcpy(block, &size, sizeof(size));
    return block + M2S_TEST_HEAP_HDR;
}

void M2S_TestHeap_Report(void)
{
    M2S_Diag_Write("M2S TEST HEAP used ");
    M2S_Diag_WriteDec((uint32_t)M2S_TestHeapUsed);
    M2S_Diag_Write(" of ");
    M2S_Diag_WriteDec((uint32_t)sizeof(M2S_TestHeapPool));
    M2S_Diag_Write(" bytes\n");
}

void *malloc(size_t size)
{
    return M2S_TestHeap_Alloc(size);
}

void free(void *ptr)
{
    (void)ptr; /* never reused */
}

void *calloc(size_t n, size_t size)
{
    void *p;

    if (size != 0U && n > SIZE_MAX / size)
    {
        return NULL;
    }
    p = M2S_TestHeap_Alloc(n * size);
    memset(p, 0, n * size); /* the pool starts zeroed, but realloc'd memory is not reused anyway */
    return p;
}

void *realloc(void *ptr, size_t size)
{
    size_t old;
    void  *p = M2S_TestHeap_Alloc(size);

    if (ptr != NULL)
    {
        memcpy(&old, (uint8_t *)ptr - M2S_TEST_HEAP_HDR, sizeof(old));
        memcpy(p, ptr, (old < size) ? old : size);
    }
    return p;
}

void *_malloc_r(struct _reent *r, size_t size)
{
    (void)r;
    return malloc(size);
}

void _free_r(struct _reent *r, void *ptr)
{
    (void)r;
    free(ptr);
}

void *_calloc_r(struct _reent *r, size_t n, size_t size)
{
    (void)r;
    return calloc(n, size);
}

void *_realloc_r(struct _reent *r, void *ptr, size_t size)
{
    (void)r;
    return realloc(ptr, size);
}
