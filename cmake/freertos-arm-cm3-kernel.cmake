# freertos-arm-cm3-kernel.cmake — m2s-cfs helper: build the pinned FreeRTOS kernel for ARM Cortex-M3
# (portable/GCC/ARM_CM3), step 08.
#
# m2s_add_freertos_arm_cm3_kernel(<target> BOARD_DIR <dir> [EXTRA_SOURCES <file>...])
#
# Creates a STATIC library <target> with the kernel sources and the unmodified ARM_CM3 port. No
# portable/MemMang/heap_*.c: static allocation only. FreeRTOSConfig.h (config/freertos/arm_cm3) takes the
# board values (clock, NVIC priority bits) from m2s_board.h in BOARD_DIR. Include directories are PUBLIC.
#
# M2S_FREERTOS_DIR and M2S_FREERTOS_CM3_CONFIG_DIR may be set before including this file; the defaults
# are this repository's layout.

get_filename_component(M2S_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED M2S_FREERTOS_DIR)
    set(M2S_FREERTOS_DIR "${M2S_ROOT}/sources/FreeRTOS-Kernel")
endif()
if(NOT DEFINED M2S_FREERTOS_CM3_CONFIG_DIR)
    set(M2S_FREERTOS_CM3_CONFIG_DIR "${M2S_ROOT}/config/freertos/arm_cm3")
endif()
set(M2S_FREERTOS_CM3_PORT_DIR "${M2S_FREERTOS_DIR}/portable/GCC/ARM_CM3")

function(m2s_add_freertos_arm_cm3_kernel TARGET)
    cmake_parse_arguments(ARG "" "BOARD_DIR" "EXTRA_SOURCES" ${ARGN})
    if(NOT ARG_BOARD_DIR OR NOT EXISTS "${ARG_BOARD_DIR}/m2s_board.h")
        message(FATAL_ERROR "m2s_add_freertos_arm_cm3_kernel: BOARD_DIR must hold m2s_board.h (got '${ARG_BOARD_DIR}')")
    endif()

    add_library(${TARGET} STATIC
        ${M2S_FREERTOS_DIR}/tasks.c
        ${M2S_FREERTOS_DIR}/list.c
        ${M2S_FREERTOS_DIR}/queue.c
        ${M2S_FREERTOS_DIR}/timers.c
        ${M2S_FREERTOS_CM3_PORT_DIR}/port.c
        ${ARG_EXTRA_SOURCES}
    )
    target_include_directories(${TARGET} PUBLIC
        ${M2S_FREERTOS_CM3_CONFIG_DIR}
        ${ARG_BOARD_DIR}
        ${M2S_FREERTOS_DIR}/include
        ${M2S_FREERTOS_CM3_PORT_DIR}
    )
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra)
endfunction()
