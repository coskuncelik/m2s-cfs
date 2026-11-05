######################################################################
#
# m2s-cfs: toolchain for cFE on FreeRTOS V11.3.2 (POSIX port) on the Linux host.
# Native 64-bit host gcc (D3). OS layer and BSP come from osal-ext/ (OSAL_EXT_SOURCE_DIR),
# the PSP from psp-ext/ through the build-time PSP copy (D27).
#
######################################################################

SET(CMAKE_SYSTEM_NAME    Linux)
SET(CMAKE_C_COMPILER     "gcc")

SET(CFE_SYSTEM_PSPNAME   "freertos-posix-sim")
SET(OSAL_SYSTEM_OSTYPE   "freertos")
SET(OSAL_SYSTEM_BSPTYPE  "freertos-posix-sim")

# OSAL reads OSAL_EXT_SOURCE_DIR from the environment or its cache; set both for this cmake process.
# M2S_ROOT_DIR comes from m2s_paths.cmake, written into the build-time copy of this directory (scripts/lib/cfe-sim.sh).
include("${CMAKE_CURRENT_LIST_DIR}/m2s_paths.cmake")
SET(ENV{OSAL_EXT_SOURCE_DIR} "${M2S_ROOT_DIR}/osal-ext")
SET(OSAL_EXT_SOURCE_DIR "${M2S_ROOT_DIR}/osal-ext" CACHE PATH "OSAL external OS/BSP sources" FORCE)
