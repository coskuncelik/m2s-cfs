######################################################################
#
# cfs-m2s-freertos: toolchain for cFE on FreeRTOS V11.3.2 (GCC/ARM_CM3 port) on an ARM Cortex-M3, board
# "qemu-mps2-an385" (QEMU machine mps2-an385). arm-none-eabi-gcc with newlib-nano, no OS underneath.
# OSAL finds the OS layer in osal/src/os/freertos and the BSP in osal/src/bsp/freertos-arm-cm3; cFE finds the
# PSP in psp/fsw/freertos-arm-cm3. Generic Cortex-M3 code (start-up, newlib stubs, allocator traps) and the
# board folders are in osal/src/bsp/freertos-arm-cm3/boards.
#
######################################################################

SET(CFE_SYSTEM_PSPNAME   "freertos-arm-cm3")
SET(OSAL_SYSTEM_OSTYPE   "freertos")
SET(OSAL_SYSTEM_BSPTYPE  "freertos-arm-cm3")

get_filename_component(M2S_CM3_BSP_DIR "${CMAKE_CURRENT_LIST_DIR}/../osal/src/bsp/freertos-arm-cm3" ABSOLUTE)
SET(M2S_CM3_COMMON_DIR "${M2S_CM3_BSP_DIR}/boards/common-cm3" CACHE PATH "Generic Cortex-M3 start-up, newlib stubs, diagnostics" FORCE)
# Board folder (linker script, console, clock). Another board: another folder under boards/ with the same files.
SET(M2S_BOARD_DIR "${M2S_CM3_BSP_DIR}/boards/qemu-mps2-an385" CACHE PATH "Board folder (linker script, console, clock)" FORCE)

# Compiler and linker settings (Cortex-M3 Thumb-2, newlib-nano, no start files, allocator traps).
include("${M2S_CM3_BSP_DIR}/cmake/arm-cm3-flags.cmake")
# The link map is written next to the executable. The link group is in arch_build_custom.cmake.
SET(CMAKE_EXE_LINKER_FLAGS_INIT "${M2S_CM3_LINK_FLAGS} -Wl,-Map=core-cpu1.map")
