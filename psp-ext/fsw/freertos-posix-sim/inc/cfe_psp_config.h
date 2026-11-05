/*
 * cfe_psp_config.h — m2s-cfs freertos-posix-sim PSP configuration.
 */
#ifndef CFE_PSP_CONFIG_H
#define CFE_PSP_CONFIG_H

#include "common_types.h"

/* Number of memory ranges in the memory range table. */
#define CFE_PSP_MEM_TABLE_SIZE 10

/* Exceptions are not captured on this platform; one entry keeps the storage types valid. */
#define CFE_PSP_MAX_EXCEPTION_ENTRIES 1

/* soft_timebase period: 10 ms, as the stock PSPs. */
#define CFE_PSP_SOFT_TIMEBASE_PERIOD 10000

/*
 * Reserved memory (boot record, exception storage, reset area, RAM disk for /ram, CDS, user area) is one
 * static array of this size (no malloc, unlike pc-rtems). The sizes cFE asks for are known only at run
 * time (GLOBAL_CONFIGDATA); start-up stops with a clear message if they do not fit.
 * Simulator value: cFE defaults need about 3.3 MiB (2 MiB RAM disk, 1 MiB user area, 128 KiB CDS, reset area).
 */
#ifndef CFE_PSP_FREERTOS_RESERVED_BYTES
#define CFE_PSP_FREERTOS_RESERVED_BYTES (4U * 1024U * 1024U)
#endif

/* Local path of the /cf fixed map (a directory of the OSAL RAM file system's root volume, D25). */
#define CFE_PSP_FREERTOS_CF_LOCAL_PATH "/cf"


typedef struct
{
    uint32 reserved; /* not used: every start is a power-on reset */
} CFE_PSP_ReservedMemoryBootRecord_t;

typedef uint32 CFE_PSP_Exception_SysTaskId_t;

typedef struct
{
    uint32 reserved; /* prevent empty structure */
} CFE_PSP_Exception_ContextDataEntry_t;

#define CFE_PSP_WATCHDOG_MIN (0)
#define CFE_PSP_WATCHDOG_MAX (0xFFFFFFFF)

#define CFE_PSP_NUM_EEPROM_BANKS 1

/* Alignment of the reserved memory blocks (128 bytes, as the stock PSPs). */
#define CFE_PSP_MEMALIGN_MASK ((cpuaddr)0x7F)

#endif /* CFE_PSP_CONFIG_H */
