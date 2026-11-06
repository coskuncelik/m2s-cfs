######################################################################
#
# m2s-cfs: cFE PSP "freertos-arm-cm3" — cFE on FreeRTOS V11.3.2 (GCC/ARM_CM3 port) on a Cortex-M3, step 08.
# Pairs with OSAL_SYSTEM_OSTYPE=freertos and OSAL_SYSTEM_BSPTYPE=freertos-arm-cm3 (same name).
# The portable parts (start-up and /cf image, reserved memory, watchdog, exceptions, SSR) are the
# freertos-posix-sim PSP sources, compiled here unchanged with this platform's cfe_psp_config.h;
# restart and panic are Cortex-M3 specific (src/cfe_psp_support.c).
#
######################################################################

set(M2S_PSP_SHARED_SRC ${CMAKE_CURRENT_LIST_DIR}/../freertos-posix-sim/src)

add_library(psp-${CFE_PSP_TARGETNAME}-impl OBJECT
    ${M2S_PSP_SHARED_SRC}/cfe_psp_start.c
    ${M2S_PSP_SHARED_SRC}/cfe_psp_memory.c
    ${M2S_PSP_SHARED_SRC}/cfe_psp_watchdog.c
    ${M2S_PSP_SHARED_SRC}/cfe_psp_exception.c
    ${M2S_PSP_SHARED_SRC}/cfe_psp_ssr.c
    src/cfe_psp_support.c
)

target_compile_definitions(psp-${CFE_PSP_TARGETNAME}-impl PRIVATE
    $<TARGET_PROPERTY:psp_module_api,INTERFACE_COMPILE_DEFINITIONS>
)

target_include_directories(psp-${CFE_PSP_TARGETNAME}-impl PRIVATE
    inc
    $<TARGET_PROPERTY:psp_module_api,INTERFACE_INCLUDE_DIRECTORIES>
    ${M2S_CM3_COMMON_DIR}   # m2s_cm3_diag.h (panic message), set by the toolchain file
)

# Project code: strict warnings.
target_compile_options(psp-${CFE_PSP_TARGETNAME}-impl PRIVATE -Wall -Wextra -Werror)
