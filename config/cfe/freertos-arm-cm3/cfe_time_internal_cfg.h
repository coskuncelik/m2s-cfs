/*
 * cfe_time_internal_cfg.h — m2s-cfs: cFE TIME sizes of the Cortex-M3 flight configuration (step 10, D36).
 * cFE's build takes this file in place of default_cfe_time_internal_cfg.h; it includes that stock file and
 * changes only the values below. Basis: scripts/10-memory-report.sh (docs/test-results/10-memory-sizing.md).
 */
#ifndef M2S_CFE_TIME_INTERNAL_CFG_H
#define M2S_CFE_TIME_INTERNAL_CFG_H

#include "default_cfe_time_internal_cfg.h"

/* 1 Hz task: measured use 860 bytes (counter_app), 1600 bytes (cFE functional tests); was 8192. */
#undef CFE_PLATFORM_TIME_ONEHZ_TASK_STACK_SIZE
#define CFE_PLATFORM_TIME_ONEHZ_TASK_STACK_SIZE 4096

#endif /* M2S_CFE_TIME_INTERNAL_CFG_H */
