cmake_minimum_required(VERSION 3.16)
project(freertos_posix_smoke C)

# Pinned FreeRTOS kernel + patched POSIX port + project config (see cmake/freertos-posix-kernel.cmake).
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/freertos-posix-kernel.cmake)
m2s_add_freertos_posix_kernel(freertos_kernel)

option(SMOKE_FORCE_HEAP_USE "Negative test: reference pvPortMalloc (must fail to link)" OFF)

add_executable(freertos_posix_smoke main.c)
target_compile_options(freertos_posix_smoke PRIVATE -Wall -Wextra -Werror)
if(SMOKE_FORCE_HEAP_USE)
    target_compile_definitions(freertos_posix_smoke PRIVATE SMOKE_FORCE_HEAP_USE)
endif()
target_link_libraries(freertos_posix_smoke PRIVATE freertos_kernel)
# Link map: evidence that no FreeRTOS heap (heap_*.c / pvPortMalloc) is linked.
target_link_options(freertos_posix_smoke PRIVATE "LINKER:-Map=${CMAKE_CURRENT_BINARY_DIR}/freertos_posix_smoke.map")
