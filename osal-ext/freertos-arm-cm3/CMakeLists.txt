######################################################################
#
# m2s-cfs: OSAL BSP "freertos-arm-cm3" — FreeRTOS V11.3.2 (GCC/ARM_CM3 port) on a Cortex-M3 (steps 08, 09).
# Generic Cortex-M3 code (start-up, newlib stubs and traps, diagnostics) comes from M2S_CM3_COMMON_DIR
# (boards/common-cm3); board specifics (linker script, console device, clock) from the board folder
# M2S_BOARD_DIR. Both are set by the mission toolchain file. Pairs with OSAL_SYSTEM_OSTYPE=freertos.
#
######################################################################

if(NOT M2S_BOARD_DIR OR NOT EXISTS "${M2S_BOARD_DIR}/m2s_board.h")
    message(FATAL_ERROR "freertos-arm-cm3 BSP: M2S_BOARD_DIR must name a board folder (got '${M2S_BOARD_DIR}')")
endif()
if(NOT M2S_CM3_COMMON_DIR OR NOT EXISTS "${M2S_CM3_COMMON_DIR}/startup_cm3.c")
    message(FATAL_ERROR "freertos-arm-cm3 BSP: M2S_CM3_COMMON_DIR must name boards/common-cm3 (got '${M2S_CM3_COMMON_DIR}')")
endif()

# Kernel + unmodified ARM_CM3 port + FreeRTOS hooks + board code (start-up, services, newlib stubs).
# All in one library: the linker pulls the start-up through ENTRY(Reset_Handler) and -u options.
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/freertos-arm-cm3-kernel.cmake)
m2s_add_freertos_arm_cm3_kernel(m2s_freertos_kernel
    BOARD_DIR ${M2S_BOARD_DIR}
    EXTRA_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/bsp_freertos_hooks.c
        ${M2S_CM3_COMMON_DIR}/startup_cm3.c
        ${M2S_CM3_COMMON_DIR}/syscalls.c
        ${M2S_CM3_COMMON_DIR}/m2s_cm3_diag.c
        ${M2S_BOARD_DIR}/board.c)
target_include_directories(m2s_freertos_kernel PUBLIC ${M2S_CM3_COMMON_DIR})

add_library(osal_freertos-arm-cm3_impl OBJECT
    src/bsp_start.c
    src/bsp_console.c
)
target_include_directories(osal_freertos-arm-cm3_impl PRIVATE
    $<TARGET_PROPERTY:m2s_freertos_kernel,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(osal_freertos-arm-cm3_impl PRIVATE -Wall -Wextra -Werror)

# Everything that links OSAL also links the kernel and the board code.
target_link_libraries(osal_public_api INTERFACE m2s_freertos_kernel)

set_property(TARGET osal_freertos-arm-cm3_impl PROPERTY OSAL_EXPECTED_OSTYPE "freertos")
