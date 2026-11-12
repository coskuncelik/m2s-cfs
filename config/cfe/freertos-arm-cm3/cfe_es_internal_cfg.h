/*
 * cfe_es_internal_cfg.h — m2s-cfs: cFE ES sizes of the Cortex-M3 flight configuration (step 10, D36).
 * cFE's build takes this file in place of default_cfe_es_internal_cfg.h; it includes that stock file and changes
 * only the values below. Basis: scripts/10-memory-report.sh (docs/test-results/10-memory-sizing.md).
 */
#ifndef M2S_CFE_ES_INTERNAL_CFG_H
#define M2S_CFE_ES_INTERNAL_CFG_H

#include "default_cfe_es_internal_cfg.h"

/* Stack of the cFE core tasks (ES, EVS, SB, TBL, TIME): largest measured use 2716 bytes (CFE_ES, -O0). */
#undef CFE_PLATFORM_ES_DEFAULT_STACK_SIZE
#define CFE_PLATFORM_ES_DEFAULT_STACK_SIZE 4096

/* Reserved memory (PSP): CDS unused by this mission (cFE minimum 8 KiB); registry >= TBL critical tables (32). */
#undef CFE_PLATFORM_ES_CDS_SIZE
#define CFE_PLATFORM_ES_CDS_SIZE (16 * 1024)
#undef CFE_PLATFORM_ES_CDS_MAX_NUM_ENTRIES
#define CFE_PLATFORM_ES_CDS_MAX_NUM_ENTRIES 32

/* User reserved memory: unused (cFE minimum 1 KiB). */
#undef CFE_PLATFORM_ES_USER_RESERVED_SIZE
#define CFE_PLATFORM_ES_USER_RESERVED_SIZE 1024

/* Volatile disk (/ram): cFE minimum 128 sectors of 512 bytes = 64 KiB; empty in the measured runs. */
#undef CFE_PLATFORM_ES_RAM_DISK_NUM_SECTORS
#define CFE_PLATFORM_ES_RAM_DISK_NUM_SECTORS 128

/* Performance log in the reset area: cFE minimum 1025 entries (was 10000, 120 KB of the 129 KB reset area). */
#undef CFE_PLATFORM_ES_PERF_DATA_BUFFER_SIZE
#define CFE_PLATFORM_ES_PERF_DATA_BUFFER_SIZE 1025

#endif /* M2S_CFE_ES_INTERNAL_CFG_H */
