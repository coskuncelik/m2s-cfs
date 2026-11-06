######################################################################
#
# m2s-cfs: toolchain for cFE on FreeRTOS V11.3.2 (GCC/ARM_CM3 port) on an ARM Cortex-M3 (step 08).
# arm-none-eabi-gcc with newlib-nano, no OS underneath. OS layer and BSP come from osal-ext/
# (OSAL_EXT_SOURCE_DIR), the PSP from psp-ext/ through the build-time PSP copy. The board folder
# (linker script, console, clock) is M2S_BOARD_DIR: the placeholder board boards/link-check (step 08) or the
# one named by the mission overlay's m2s_board.cmake (QEMU mps2-an385, step 09).
#
######################################################################

SET(CMAKE_SYSTEM_NAME      Generic)
SET(CMAKE_SYSTEM_PROCESSOR arm)
SET(CMAKE_C_COMPILER       "arm-none-eabi-gcc")
# The compiler checks cannot link a program without the board's start-up and linker script.
SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

SET(CFE_SYSTEM_PSPNAME   "freertos-arm-cm3")
SET(OSAL_SYSTEM_OSTYPE   "freertos")
SET(OSAL_SYSTEM_BSPTYPE  "freertos-arm-cm3")

# M2S_ROOT_DIR comes from m2s_paths.cmake, written into the build-time copy of this directory.
include("${CMAKE_CURRENT_LIST_DIR}/m2s_paths.cmake")
SET(ENV{OSAL_EXT_SOURCE_DIR} "${M2S_ROOT_DIR}/osal-ext")
SET(OSAL_EXT_SOURCE_DIR "${M2S_ROOT_DIR}/osal-ext" CACHE PATH "OSAL external OS/BSP sources" FORCE)
# Board folder (linker script, console, clock): the placeholder board unless a mission overlay provides
# m2s_board.cmake (e.g. config/cfe/freertos-arm-cm3-qemu). Generic Cortex-M3 code: boards/common-cm3.
SET(M2S_CM3_COMMON_DIR "${M2S_ROOT_DIR}/boards/common-cm3" CACHE PATH "Generic Cortex-M3 start-up, newlib stubs, diagnostics" FORCE)
include("${CMAKE_CURRENT_LIST_DIR}/m2s_board.cmake" OPTIONAL)
if(NOT DEFINED M2S_BOARD_NAME)
    SET(M2S_BOARD_NAME link-check)
endif()
SET(M2S_BOARD_DIR "${M2S_ROOT_DIR}/boards/${M2S_BOARD_NAME}" CACHE PATH "Board folder (linker script, console, clock)" FORCE)

# Cortex-M3, Thumb-2, no FPU. Sections per function/object so unused code is dropped at link time.
# nano.specs (newlib-nano headers and libraries) is in the C flags only: CMake also puts the C flags on the
# link line, and gcc rejects the specs file given twice.
SET(M2S_CM3_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft")
SET(CMAKE_C_FLAGS_INIT "${M2S_CM3_FLAGS} --specs=nano.specs -ffunction-sections -fdata-sections")
# -nostartfiles: the board's reset handler is the start-up (no newlib crt0). The map file is written next
# to the executable. -u keeps the start-up object (vector table) even though nothing calls it.
# The allocator symbols are also marked undefined from the start, so the linker takes them from the
# board's traps (syscalls.c, in the board library, which comes before the C library on the link line)
# before anything in newlib asks for them; newlib's own allocator objects are then never pulled in.
SET(M2S_ALLOC_TRAPS "-Wl,-u,malloc -Wl,-u,free -Wl,-u,calloc -Wl,-u,realloc -Wl,-u,_malloc_r -Wl,-u,_free_r -Wl,-u,_calloc_r -Wl,-u,_realloc_r")
SET(CMAKE_EXE_LINKER_FLAGS_INIT
    "${M2S_CM3_FLAGS} -nostartfiles -T${M2S_BOARD_DIR}/board.ld -Wl,--gc-sections -Wl,-Map=core-cpu1.map -Wl,-u,Reset_Handler -Wl,-u,M2S_VectorTable ${M2S_ALLOC_TRAPS}")
# The C library goes inside the link group (arch_build_custom.cmake), so its references to the board's
# newlib stubs (e.g. _sbrk) resolve against the board library.
SET(CMAKE_C_STANDARD_LIBRARIES "-lc -lm -lgcc")
