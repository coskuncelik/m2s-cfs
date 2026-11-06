# toolchain.cmake — m2s-cfs: standalone OSAL build for the Cortex-M3 (OSAL functional tests, step 09b).
#
# Used with cmake -S sources/cFS/osal -DCMAKE_TOOLCHAIN_FILE=<this file> -DOSAL_SYSTEM_BSPTYPE=freertos-arm-cm3
# (see scripts/09b-run-osal-tests-arm-cm3-qemu.sh). Board: QEMU mps2-an385 unless M2S_BOARD_NAME is given.
# These are TEST images: the static test-only allocator (test_heap.c, D33) replaces the allocator traps,
# because OSAL's test library ut_assert allocates with malloc.

get_filename_component(M2S_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT DEFINED M2S_BOARD_NAME)
    SET(M2S_BOARD_NAME qemu-mps2-an385)
endif()
SET(M2S_BOARD_DIR "${M2S_ROOT_DIR}/boards/${M2S_BOARD_NAME}" CACHE PATH "Board folder (linker script, console, clock)" FORCE)
SET(M2S_CM3_COMMON_DIR "${M2S_ROOT_DIR}/boards/common-cm3" CACHE PATH "Generic Cortex-M3 start-up, newlib stubs, diagnostics" FORCE)
SET(M2S_CM3_TEST_HEAP ON CACHE BOOL "Link the test-only allocator (test images only, never flight)" FORCE)

include("${M2S_ROOT_DIR}/cmake/arm-cm3-flags.cmake")
SET(CMAKE_EXE_LINKER_FLAGS_INIT "${M2S_CM3_LINK_FLAGS}")
# Libraries in one GNU ld group (OSAL, BSP, kernel/board code and the C library reference each other);
# one link map per test program, next to it.
SET(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,-Map=<TARGET>.map -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")
