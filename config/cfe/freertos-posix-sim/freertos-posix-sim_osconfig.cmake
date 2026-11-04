# m2s-cfs: OSAL configuration for cFE on the FreeRTOS simulator — the same as for the OSAL tests (step 04).
get_filename_component(M2S_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${M2S_ROOT_DIR}/config/osal/freertos-posix-sim/osal_config.cmake")
