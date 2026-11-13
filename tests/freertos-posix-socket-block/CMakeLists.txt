# m2s-cfs step 12a phase 1: can a FreeRTOS POSIX-port task wait in a host socket call? (D41)
cmake_minimum_required(VERSION 3.16)
project(freertos_posix_socket_block C)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/freertos-posix-kernel.cmake)
m2s_add_freertos_posix_kernel(freertos_kernel)

add_executable(socket_block main.c)
target_compile_options(socket_block PRIVATE -Wall -Wextra -Werror)
target_link_libraries(socket_block PRIVATE freertos_kernel)
