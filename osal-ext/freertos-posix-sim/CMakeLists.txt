######################################################################
#
# m2s-cfs: OSAL BSP "freertos-posix-sim" — FreeRTOS V11.3.2 on the Linux host (POSIX port).
# Found by OSAL through OSAL_EXT_SOURCE_DIR. Pairs with OSAL_SYSTEM_OSTYPE=freertos.
#
######################################################################

# Kernel + patched POSIX port + project hooks (shared with tests/freertos-posix-smoke).
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/freertos-posix-kernel.cmake)
m2s_add_freertos_posix_kernel(m2s_freertos_kernel
    EXTRA_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/bsp_freertos_hooks.c)

add_library(osal_freertos-posix-sim_impl OBJECT
    src/bsp_start.c
    src/bsp_console.c
)
target_include_directories(osal_freertos-posix-sim_impl PRIVATE
    $<TARGET_PROPERTY:m2s_freertos_kernel,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(osal_freertos-posix-sim_impl PRIVATE -Wall -Wextra -Werror)

# Everything that links OSAL (library, tests, later cFE) also links the kernel.
target_link_libraries(osal_public_api INTERFACE m2s_freertos_kernel)

set_property(TARGET osal_freertos-posix-sim_impl PROPERTY OSAL_EXPECTED_OSTYPE "freertos")
