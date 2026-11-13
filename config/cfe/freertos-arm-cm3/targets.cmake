######################################################################
#
# m2s-cfs: cFE mission targets — one CPU, cFE on FreeRTOS V11.3.2 on an ARM Cortex-M3 (step 08).
# Passed as MISSION_DEFS by scripts/08 (scripts/lib/cfe-sim.sh). Same apps and /cf image as the
# simulator mission (config/cfe/freertos-posix-sim).
#
######################################################################

SET(MISSION_NAME "M2SMission")
SET(SPACECRAFT_ID 0x42)

SET(MISSION_CPUNAMES cpu1)

# cFE adds cfe_assert (a dynamically loaded library) to every mission; a bare-metal target cannot load
# modules, so it is removed (as in the functional-test mission, which links it statically).
list(REMOVE_ITEM MISSION_GLOBAL_APPLIST cfe_assert)

SET(cpu1_PROCESSORID 1)
SET(cpu1_SYSTEM freertos-arm-cm3)     # toolchain-freertos-arm-cm3.cmake
SET(cpu1_APPLIST)                     # no dynamically loaded apps
SET(cpu1_FILELIST)

# Apps linked into core-cpu1; the symbol list feeds OSAL's static loader (module name = cFE name).
SET(cpu1_STATIC_APPLIST sample_lib counter_app sch_lab)
SET(cpu1_STATIC_SYMLIST "SAMPLE_LIB_Init,SAMPLE_LIB" "COUNTER_APP_Main,COUNTER_APP" "SCH_LAB_AppMain,SCH_LAB_APP")

# /cf image embedded in the executable; the PSP unpacks it into /cf at start-up.
SET(cpu1_EMBED_FILELIST "CFE_PSP_CF_IMAGE,cf.tar")
