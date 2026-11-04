######################################################################
#
# m2s-cfs: cFE PSP "freertos-posix-sim" — cFE on FreeRTOS V11.3.2 (POSIX port) on the Linux host.
# Pairs with OSAL_SYSTEM_OSTYPE=freertos and OSAL_SYSTEM_BSPTYPE=freertos-posix-sim (same name).
# Built from a build-time copy of the stock PSP tree (scripts/05, D27); sources/ is never written.
#
######################################################################

add_library(psp-${CFE_PSP_TARGETNAME}-impl OBJECT
    src/cfe_psp_start.c
    src/cfe_psp_memory.c
    src/cfe_psp_support.c
    src/cfe_psp_watchdog.c
    src/cfe_psp_exception.c
    src/cfe_psp_ssr.c
)

target_compile_definitions(psp-${CFE_PSP_TARGETNAME}-impl PRIVATE
    $<TARGET_PROPERTY:psp_module_api,INTERFACE_COMPILE_DEFINITIONS>
)

target_include_directories(psp-${CFE_PSP_TARGETNAME}-impl PRIVATE
    inc
    $<TARGET_PROPERTY:psp_module_api,INTERFACE_INCLUDE_DIRECTORIES>
)

# Project code: strict warnings.
target_compile_options(psp-${CFE_PSP_TARGETNAME}-impl PRIVATE -Wall -Wextra -Werror)
