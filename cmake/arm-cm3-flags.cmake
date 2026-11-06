# arm-cm3-flags.cmake — m2s-cfs: compiler and linker settings for every Cortex-M3 build (cFE missions,
# step 08/09; standalone OSAL tests, step 09b). Included from a toolchain file after M2S_BOARD_DIR is set.
#
# arm-none-eabi-gcc, newlib-nano, Cortex-M3 Thumb-2 without FPU, CMake platform Generic. The caller adds the
# link map option (its file name differs per build).

if(NOT M2S_BOARD_DIR OR NOT EXISTS "${M2S_BOARD_DIR}/board.ld")
    message(FATAL_ERROR "arm-cm3-flags.cmake: set M2S_BOARD_DIR to a board folder first (got '${M2S_BOARD_DIR}')")
endif()

SET(CMAKE_SYSTEM_NAME      Generic)
SET(CMAKE_SYSTEM_PROCESSOR arm)
SET(CMAKE_C_COMPILER       "arm-none-eabi-gcc")
# The compiler checks cannot link a program without the board's start-up and linker script.
SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-M3, Thumb-2, no FPU. Sections per function/object so unused code is dropped at link time.
# nano.specs (newlib-nano headers and libraries) is in the C flags only: CMake also puts the C flags on the
# link line, and gcc rejects the specs file given twice.
SET(M2S_CM3_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft")
SET(CMAKE_C_FLAGS_INIT "${M2S_CM3_FLAGS} --specs=nano.specs -ffunction-sections -fdata-sections")
# -nostartfiles: the start-up's reset handler is the entry (no newlib crt0). -u keeps the start-up object
# (vector table) even though nothing calls it. The allocator symbols are also marked undefined from the
# start, so the linker takes them from the kernel/board library (alloc_traps.c in flight images,
# test_heap.c in test images), which comes before the C library on the link line; newlib's own allocator
# objects are then never pulled in.
SET(M2S_ALLOC_TRAPS "-Wl,-u,malloc -Wl,-u,free -Wl,-u,calloc -Wl,-u,realloc -Wl,-u,_malloc_r -Wl,-u,_free_r -Wl,-u,_calloc_r -Wl,-u,_realloc_r")
SET(M2S_CM3_LINK_FLAGS
    "${M2S_CM3_FLAGS} -nostartfiles -T${M2S_BOARD_DIR}/board.ld -Wl,--gc-sections -Wl,-u,Reset_Handler -Wl,-u,M2S_VectorTable ${M2S_ALLOC_TRAPS}")
# The C library goes inside the link group, so its references to the newlib stubs (e.g. _write) resolve
# against the board library.
SET(CMAKE_C_STANDARD_LIBRARIES "-lc -lm -lgcc")
