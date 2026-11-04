/*
 * cfe_psp_memory.c — m2s-cfs freertos-posix-sim PSP: reserved memory, CDS, text segments.
 *
 * All reserved areas live in one static array (CFE_PSP_FREERTOS_RESERVED_BYTES), laid out at start-up
 * from the sizes cFE reports in GLOBAL_CONFIGDATA. No malloc (pc-rtems mallocs this block).
 */
#include <string.h>

#include "common_types.h"
#include "osapi.h"
#include "cfe_psp.h"
#include "cfe_psp_config.h"
#include "cfe_psp_memory.h"
#include "target_config.h"

#define CFE_PSP_CDS_SIZE             (GLOBAL_CONFIGDATA.CfeConfig->CdsSize)
#define CFE_PSP_RESET_AREA_SIZE      (GLOBAL_CONFIGDATA.CfeConfig->ResetAreaSize)
#define CFE_PSP_USER_RESERVED_SIZE   (GLOBAL_CONFIGDATA.CfeConfig->UserReservedSize)
#define CFE_PSP_RAM_DISK_SECTOR_SIZE (GLOBAL_CONFIGDATA.CfeConfig->RamDiskSectorSize)
#define CFE_PSP_RAM_DISK_NUM_SECTORS (GLOBAL_CONFIGDATA.CfeConfig->RamDiskTotalSectors)

typedef struct
{
    CFE_PSP_ReservedMemoryBootRecord_t BootRecord;
    CFE_PSP_ExceptionStorage_t         ExceptionStorage;
} CFE_PSP_FreeRTOS_FixedLayout_t;

CFE_PSP_ReservedMemoryMap_t CFE_PSP_ReservedMemoryMap = {0};

/* 128-byte alignment, matching CFE_PSP_MEMALIGN_MASK. */
static uint8 CFE_PSP_FreeRTOS_ReservedMem[CFE_PSP_FREERTOS_RESERVED_BYTES] __attribute__((aligned(128)));
static size_t CFE_PSP_FreeRTOS_ReservedUsed;

static size_t CFE_PSP_Align(size_t size)
{
    return (size + CFE_PSP_MEMALIGN_MASK) & ~(size_t)CFE_PSP_MEMALIGN_MASK;
}

/* Takes the next block of the static area. Stops start-up if it does not fit. */
static void *CFE_PSP_TakeBlock(size_t size)
{
    void *block;

    size = CFE_PSP_Align(size);
    if (size > sizeof(CFE_PSP_FreeRTOS_ReservedMem) - CFE_PSP_FreeRTOS_ReservedUsed)
    {
        OS_printf("CFE_PSP: reserved memory too small: %lu bytes used, %lu more needed, %lu available "
                  "(CFE_PSP_FREERTOS_RESERVED_BYTES)\n",
                  (unsigned long)CFE_PSP_FreeRTOS_ReservedUsed, (unsigned long)size,
                  (unsigned long)sizeof(CFE_PSP_FreeRTOS_ReservedMem));
        CFE_PSP_Panic(CFE_PSP_ERROR);
    }
    block = &CFE_PSP_FreeRTOS_ReservedMem[CFE_PSP_FreeRTOS_ReservedUsed];
    CFE_PSP_FreeRTOS_ReservedUsed += size;
    return block;
}

void CFE_PSP_SetupReservedMemoryMap(void)
{
    CFE_PSP_FreeRTOS_FixedLayout_t *fixed;
    size_t                          disk_size;

    CFE_PSP_FreeRTOS_ReservedUsed = 0;
    disk_size = (size_t)CFE_PSP_RAM_DISK_SECTOR_SIZE * (size_t)CFE_PSP_RAM_DISK_NUM_SECTORS;

    fixed = CFE_PSP_TakeBlock(sizeof(*fixed));
    CFE_PSP_ReservedMemoryMap.BootPtr             = &fixed->BootRecord;
    CFE_PSP_ReservedMemoryMap.ExceptionStoragePtr = &fixed->ExceptionStorage;

    CFE_PSP_ReservedMemoryMap.ResetMemory.BlockPtr  = CFE_PSP_TakeBlock(CFE_PSP_RESET_AREA_SIZE);
    CFE_PSP_ReservedMemoryMap.ResetMemory.BlockSize = CFE_PSP_RESET_AREA_SIZE;

    CFE_PSP_ReservedMemoryMap.VolatileDiskMemory.BlockPtr  = CFE_PSP_TakeBlock(disk_size);
    CFE_PSP_ReservedMemoryMap.VolatileDiskMemory.BlockSize = disk_size;

    CFE_PSP_ReservedMemoryMap.CDSMemory.BlockPtr  = CFE_PSP_TakeBlock(CFE_PSP_CDS_SIZE);
    CFE_PSP_ReservedMemoryMap.CDSMemory.BlockSize = CFE_PSP_CDS_SIZE;

    CFE_PSP_ReservedMemoryMap.UserReservedMemory.BlockPtr  = CFE_PSP_TakeBlock(CFE_PSP_USER_RESERVED_SIZE);
    CFE_PSP_ReservedMemoryMap.UserReservedMemory.BlockSize = CFE_PSP_USER_RESERVED_SIZE;

    OS_printf("CFE_PSP: reserved memory: %lu of %lu static bytes used\n", (unsigned long)CFE_PSP_FreeRTOS_ReservedUsed,
              (unsigned long)sizeof(CFE_PSP_FreeRTOS_ReservedMem));

    /* One RAM entry over the whole address space, so CFE_PSP_ValidateMemRange() works (as pc-rtems). */
    CFE_PSP_MemRangeSet(0, CFE_PSP_MEM_RAM, 0, SIZE_MAX, CFE_PSP_MEM_SIZE_DWORD, CFE_PSP_MEM_ATTR_READWRITE);
}

int32 CFE_PSP_InitProcessorReservedMemory(uint32 RestartType)
{
    (void)RestartType; /* always a power-on reset on the simulator */
    OS_printf("CFE_PSP: clearing processor reserved memory\n");
    memset(CFE_PSP_FreeRTOS_ReservedMem, 0, sizeof(CFE_PSP_FreeRTOS_ReservedMem));
    return CFE_PSP_SUCCESS;
}

void CFE_PSP_DeleteProcessorReservedMemory(void)
{
    /* static memory: nothing to release */
}

static int32 CFE_PSP_GetBlock(const CFE_PSP_MemoryBlock_t *block, cpuaddr *ptr, uint32 *size)
{
    if (ptr == NULL || size == NULL)
    {
        return CFE_PSP_ERROR;
    }
    *ptr  = (cpuaddr)block->BlockPtr;
    *size = (uint32)block->BlockSize;
    return CFE_PSP_SUCCESS;
}

int32 CFE_PSP_GetResetArea(cpuaddr *PtrToResetArea, uint32 *SizeOfResetArea)
{
    return CFE_PSP_GetBlock(&CFE_PSP_ReservedMemoryMap.ResetMemory, PtrToResetArea, SizeOfResetArea);
}

int32 CFE_PSP_GetUserReservedArea(cpuaddr *PtrToUserArea, uint32 *SizeOfUserArea)
{
    return CFE_PSP_GetBlock(&CFE_PSP_ReservedMemoryMap.UserReservedMemory, PtrToUserArea, SizeOfUserArea);
}

int32 CFE_PSP_GetVolatileDiskMem(cpuaddr *PtrToVolDisk, uint32 *SizeOfVolDisk)
{
    return CFE_PSP_GetBlock(&CFE_PSP_ReservedMemoryMap.VolatileDiskMemory, PtrToVolDisk, SizeOfVolDisk);
}

int32 CFE_PSP_GetCDSSize(uint32 *SizeOfCDS)
{
    if (SizeOfCDS == NULL)
    {
        return CFE_PSP_ERROR;
    }
    *SizeOfCDS = (uint32)CFE_PSP_ReservedMemoryMap.CDSMemory.BlockSize;
    return CFE_PSP_SUCCESS;
}

/* Offset/length check without overflow. */
static bool CFE_PSP_CDSRangeOk(uint32 offset, uint32 count)
{
    size_t size = CFE_PSP_ReservedMemoryMap.CDSMemory.BlockSize;

    return offset < size && count <= size - offset;
}

int32 CFE_PSP_WriteToCDS(const void *PtrToDataToWrite, uint32 CDSOffset, uint32 NumBytes)
{
    if (PtrToDataToWrite == NULL || !CFE_PSP_CDSRangeOk(CDSOffset, NumBytes))
    {
        return CFE_PSP_ERROR;
    }
    memcpy((uint8 *)CFE_PSP_ReservedMemoryMap.CDSMemory.BlockPtr + CDSOffset, PtrToDataToWrite, NumBytes);
    return CFE_PSP_SUCCESS;
}

int32 CFE_PSP_ReadFromCDS(void *PtrToDataToRead, uint32 CDSOffset, uint32 NumBytes)
{
    if (PtrToDataToRead == NULL || !CFE_PSP_CDSRangeOk(CDSOffset, NumBytes))
    {
        return CFE_PSP_ERROR;
    }
    memcpy(PtrToDataToRead, (const uint8 *)CFE_PSP_ReservedMemoryMap.CDSMemory.BlockPtr + CDSOffset, NumBytes);
    return CFE_PSP_SUCCESS;
}

/* Text segment locations are not known on the simulator (as pc-linux). */
int32 CFE_PSP_GetKernelTextSegmentInfo(cpuaddr *PtrToKernelSegment, uint32 *SizeOfKernelSegment)
{
    (void)PtrToKernelSegment;
    (void)SizeOfKernelSegment;
    return CFE_PSP_ERROR_NOT_IMPLEMENTED;
}

int32 CFE_PSP_GetCFETextSegmentInfo(cpuaddr *PtrToCFESegment, uint32 *SizeOfCFESegment)
{
    (void)PtrToCFESegment;
    (void)SizeOfCFESegment;
    return CFE_PSP_ERROR_NOT_IMPLEMENTED;
}
