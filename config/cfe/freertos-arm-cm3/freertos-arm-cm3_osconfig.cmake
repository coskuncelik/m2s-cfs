# m2s-cfs: OSAL configuration for cFE on FreeRTOS Cortex-M3 (step 08) — the same as on the simulator.
include("${CMAKE_CURRENT_LIST_DIR}/m2s_paths.cmake")
include("${M2S_ROOT_DIR}/config/osal/freertos-arm-cm3/osal_config.cmake")
# Flight sizes of the OSAL limits (step 10, D36); a mission laid over this one (e.g. the functional-test
# mission freertos-arm-cm3-qemu-ft) replaces m2s_osal_sizing.cmake to keep the default sizes.
include("${CMAKE_CURRENT_LIST_DIR}/m2s_osal_sizing.cmake")
