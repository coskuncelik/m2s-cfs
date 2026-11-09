######################################################################
#
# cfs-m2s-freertos: toolchain for cFE on FreeRTOS V11.3.2 (POSIX port) on the Linux host.
# Native 64-bit host gcc. OSAL finds the OS layer in osal/src/os/freertos and the BSP in
# osal/src/bsp/freertos-posix-sim; cFE finds the PSP in psp/fsw/freertos-posix-sim.
#
######################################################################

SET(CMAKE_SYSTEM_NAME    Linux)
SET(CMAKE_C_COMPILER     "gcc")

SET(CFE_SYSTEM_PSPNAME   "freertos-posix-sim")
SET(OSAL_SYSTEM_OSTYPE   "freertos")
SET(OSAL_SYSTEM_BSPTYPE  "freertos-posix-sim")
