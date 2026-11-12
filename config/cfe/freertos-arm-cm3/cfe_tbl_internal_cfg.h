/*
 * cfe_tbl_internal_cfg.h — m2s-cfs: cFE TBL sizes of the Cortex-M3 flight configuration (step 10, D36).
 * cFE's build takes this file in place of default_cfe_tbl_internal_cfg.h; it includes that stock file and changes
 * only the values below. Basis: scripts/10-memory-report.sh (docs/test-results/10-memory-sizing.md).
 */
#ifndef M2S_CFE_TBL_INTERNAL_CFG_H
#define M2S_CFE_TBL_INTERNAL_CFG_H

#include "default_cfe_tbl_internal_cfg.h"

/* Two table loads at a time (was 4): TBL takes this many 16 KiB load buffers from its pool at start. */
#undef CFE_PLATFORM_TBL_MAX_SIMULTANEOUS_LOADS
#define CFE_PLATFORM_TBL_MAX_SIMULTANEOUS_LOADS 2

/* Pool: 2 load buffers (32 KiB) + 32 KiB for tables. cFE checks pool >= load buffers + one 16 KiB table. */
#undef CFE_PLATFORM_TBL_BUF_MEMORY_BYTES
#define CFE_PLATFORM_TBL_BUF_MEMORY_BYTES (64 * 1024)

/* Registry and handles: no table in this mission (was 128 tables, 256 handles). */
#undef CFE_PLATFORM_TBL_MAX_NUM_TABLES
#define CFE_PLATFORM_TBL_MAX_NUM_TABLES 32
#undef CFE_PLATFORM_TBL_MAX_NUM_HANDLES
#define CFE_PLATFORM_TBL_MAX_NUM_HANDLES 64

#endif /* M2S_CFE_TBL_INTERNAL_CFG_H */
