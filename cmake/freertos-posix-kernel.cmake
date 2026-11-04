# freertos-posix-kernel.cmake — m2s-cfs helper: build the pinned FreeRTOS kernel for the Linux simulator.
#
# m2s_add_freertos_posix_kernel(<target> [EXTRA_SOURCES <file>...])
#
# Creates a STATIC library <target> with the kernel sources, a PATCHED COPY of the POSIX port
# (patches/freertos-kernel/0001-posix-port-tick-catch-up.patch, applied with `patch -o` into the
# build directory; sources/ is never modified) and the project hooks. No portable/MemMang/heap_*.c:
# static allocation only. Include directories and Threads are PUBLIC.

get_filename_component(M2S_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(M2S_FREERTOS_DIR "${M2S_ROOT}/sources/FreeRTOS-Kernel")
set(M2S_FREERTOS_CONFIG_DIR "${M2S_ROOT}/config/freertos/posix")
set(M2S_FREERTOS_PORT_DIR "${M2S_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix")
set(M2S_FREERTOS_PORT_PATCH "${M2S_ROOT}/patches/freertos-kernel/0001-posix-port-tick-catch-up.patch")

function(m2s_add_freertos_posix_kernel TARGET)
    cmake_parse_arguments(ARG "" "" "EXTRA_SOURCES" ${ARGN})

    find_program(PATCH_EXECUTABLE patch REQUIRED)
    find_package(Threads REQUIRED)

    set(patched_dir "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-patched")
    set(patched_port "${patched_dir}/port.c")
    add_custom_command(
        OUTPUT ${patched_port}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${patched_dir}
        COMMAND ${PATCH_EXECUTABLE} --quiet --forward -o ${patched_port}
                ${M2S_FREERTOS_PORT_DIR}/port.c ${M2S_FREERTOS_PORT_PATCH}
        DEPENDS ${M2S_FREERTOS_PORT_DIR}/port.c ${M2S_FREERTOS_PORT_PATCH}
        COMMENT "Applying 0001-posix-port-tick-catch-up.patch to a copy of port.c (${TARGET})"
        VERBATIM)

    add_library(${TARGET} STATIC
        ${M2S_FREERTOS_DIR}/tasks.c
        ${M2S_FREERTOS_DIR}/list.c
        ${M2S_FREERTOS_DIR}/queue.c
        ${M2S_FREERTOS_DIR}/timers.c
        ${patched_port}
        ${M2S_FREERTOS_PORT_DIR}/utils/wait_for_event.c
        ${M2S_FREERTOS_CONFIG_DIR}/freertos_posix_hooks.c
        ${ARG_EXTRA_SOURCES}
    )
    target_include_directories(${TARGET} PUBLIC
        ${M2S_FREERTOS_CONFIG_DIR}
        ${M2S_FREERTOS_DIR}/include
        ${M2S_FREERTOS_PORT_DIR}
        ${M2S_FREERTOS_PORT_DIR}/utils
    )
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra)
    # The POSIX port needs POSIX/GNU host APIs (robust mutexes, clock_gettime). cFE builds all target code
    # with -std=c99, which hides them, so request them explicitly. Host-only (simulator kernel).
    # Empty value, identical to the "#define _GNU_SOURCE" in port.c (no redefinition warning).
    target_compile_definitions(${TARGET} PRIVATE _GNU_SOURCE=)
    target_link_libraries(${TARGET} PUBLIC Threads::Threads)
endfunction()
