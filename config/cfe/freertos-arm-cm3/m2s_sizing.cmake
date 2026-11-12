# m2s-cfs: memory sizes of the OSAL FreeRTOS layer and PSP for the Cortex-M3 flight configuration
# (step 10, D36). Included by arch_build_custom.cmake. The BSP main task keeps its 16 KiB stack: its source
# (osal-ext/freertos-arm-cm3/src/bsp_start.c) is part of the published bundle, which this step leaves unchanged. Basis: scripts/10-memory-report.sh (largest measured
# task stack 2716 bytes at -O0, timebase helper 364; see docs/test-results/10-memory-sizing.md).
# cFE's own sizes are in cfe_*_internal_cfg.h next to this file.
# Task stack pools X(bytes, count): every task requests at most 4 KiB (cFE overrides, startup script); 9 tasks
# in use. A function-like macro: given as a compile option (COMPILE_DEFINITIONS does not take function-style ones).
add_compile_options("-DOS_FREERTOS_STACK_POOLS(X)=X(4096,12)X(8192,2)")
add_definitions(
    -DOS_FREERTOS_QUEUE_STORAGE_BYTES=1024      # deepest queue: depth 32 of 4-byte messages = 272 bytes
    -DOS_FREERTOS_TIMEBASE_STACK_BYTES=2048
    -DOS_FREERTOS_RAMFS_POOL_BYTES=4096U        # RAM disks without caller memory: none in this mission
    -DOS_FREERTOS_RAMFS_ROOT_BYTES=16384U       # /cf (startup script, 422 bytes)
    -DCFE_PSP_FREERTOS_RESERVED_BYTES=131072U   # reset area + volatile disk + CDS + user reserved (cFE sizes)
)
