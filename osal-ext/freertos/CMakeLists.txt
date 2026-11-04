######################################################################
#
# m2s-cfs: OSAL implementation layer for FreeRTOS (OSAL_SYSTEM_OSTYPE=freertos)
# Found by OSAL through OSAL_EXT_SOURCE_DIR (see docs/DECISIONS.md D17).
#
######################################################################

add_library(osal_freertos_impl OBJECT
    src/os-impl-binsem.c
    src/os-impl-clock.c
    src/os-impl-common.c
    src/os-impl-console.c
    src/os-impl-countsem.c
    src/os-impl-idmap.c
    src/os-impl-misc.c
    src/os-impl-mutex.c
    src/os-impl-queues.c
    src/os-impl-sem.c
    src/os-impl-tasks.c
    src/os-impl-timebase.c
    src/os-impl-filesys.c                   # static RAM file system (04b)
    src/os-ramfs.c

    # OSAL portable blocks
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-console-bsp.c
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-no-condvar.c
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-no-shell.c
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-no-loader.c
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-no-symtab.c
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-no-network.c
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-no-sockets.c
    ${OSAL_SOURCE_DIR}/src/os/portable/os-impl-no-select.c
)

if (OSAL_CONFIG_INCLUDE_NETWORK OR OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER)
    message(FATAL_ERROR "OSAL freertos layer: set OSAL_CONFIG_INCLUDE_NETWORK and OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER to FALSE")
endif()

target_include_directories(osal_freertos_impl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)
# FreeRTOS headers and FreeRTOSConfig.h come from the kernel library the BSP defines.
target_include_directories(osal_freertos_impl PRIVATE $<TARGET_PROPERTY:m2s_freertos_kernel,INTERFACE_INCLUDE_DIRECTORIES>)
# Strict warnings for project code only; OSAL's portable blocks are compiled as upstream ships them.
set_source_files_properties(
    src/os-impl-binsem.c src/os-impl-clock.c src/os-impl-common.c src/os-impl-console.c
    src/os-impl-countsem.c src/os-impl-idmap.c src/os-impl-misc.c src/os-impl-mutex.c
    src/os-impl-queues.c src/os-impl-sem.c src/os-impl-tasks.c src/os-impl-timebase.c
    src/os-impl-filesys.c src/os-ramfs.c
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
