######################################################################
#
# m2s-cfs: toolchain for cFE on FreeRTOS V11.3.2 (GCC/ARM_CM3 port) on an ARM Cortex-M3 (step 08).
# arm-none-eabi-gcc with newlib-nano, no OS underneath. OS layer and BSP come from osal-ext/
# (OSAL_EXT_SOURCE_DIR), the PSP from psp-ext/ through the build-time PSP copy. The board folder
# (linker script, console, clock) is M2S_BOARD_DIR: the placeholder board boards/link-check (step 08) or the
# one named by the mission overlay's m2s_board.cmake (QEMU mps2-an385, step 09).
#
######################################################################

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

# Compiler and linker settings shared with the standalone OSAL test build (cmake/arm-cm3-flags.cmake).
include("${M2S_ROOT_DIR}/cmake/arm-cm3-flags.cmake")
# The map file is written next to the executable. The link group is in arch_build_custom.cmake.
SET(CMAKE_EXE_LINKER_FLAGS_INIT "${M2S_CM3_LINK_FLAGS} -Wl,-Map=core-cpu1.map")
