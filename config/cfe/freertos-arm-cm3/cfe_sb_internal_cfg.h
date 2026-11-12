/*
 * cfe_sb_internal_cfg.h — m2s-cfs: cFE SB sizes of the Cortex-M3 flight configuration (step 10, D36).
 * cFE's build takes this file in place of default_cfe_sb_internal_cfg.h; it includes that stock file and changes
 * only the values below. Basis: scripts/10-memory-report.sh (docs/test-results/10-memory-sizing.md).
 */
#ifndef M2S_CFE_SB_INTERNAL_CFG_H
#define M2S_CFE_SB_INTERNAL_CFG_H

#include "default_cfe_sb_internal_cfg.h"

/* Pipes: 5 in use; must not exceed OS_MAX_QUEUES (16, m2s_osal_sizing.cmake). */
#undef CFE_PLATFORM_SB_MAX_PIPES
#define CFE_PLATFORM_SB_MAX_PIPES 16

/* Buffer pool: counter_app peak 1,264 bytes; room for one maximum-size (32 KiB) message plus traffic. */
#undef CFE_PLATFORM_SB_BUF_MEMORY_BYTES
#define CFE_PLATFORM_SB_BUF_MEMORY_BYTES (64 * 1024)

#endif /* M2S_CFE_SB_INTERNAL_CFG_H */
