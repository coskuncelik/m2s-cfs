######################################################################
#
# cfs-m2s-freertos: cFE mission targets — one CPU, cFE on FreeRTOS V11.3.2 (POSIX port) on the Linux host.
# Replaces the stock sample_defs/targets.cmake; see PORTING.md.
#
######################################################################

SET(MISSION_NAME "M2SMission")
SET(SPACECRAFT_ID 0x42)

SET(MISSION_CPUNAMES cpu1)

SET(cpu1_PROCESSORID 1)
SET(cpu1_SYSTEM freertos-posix-sim)   # toolchain-freertos-posix-sim.cmake
SET(cpu1_APPLIST)                     # no dynamically loaded apps (the OSAL dynamic loader is off)
SET(cpu1_FILELIST)

# Apps linked into core-cpu1. The symbol list feeds OSAL's static loader: the module name must equal the
# cFE name in the startup script (4th field), e.g. COUNTER_APP.
SET(cpu1_STATIC_APPLIST sample_lib counter_app)
SET(cpu1_STATIC_SYMLIST "SAMPLE_LIB_Init,SAMPLE_LIB" "COUNTER_APP_Main,COUNTER_APP")

# /cf image embedded in the executable; the PSP unpacks it into /cf at start-up. cf.tar is generated at
# configure time by mission_build_custom.cmake from cpu1_cfe_es_startup.scr.
SET(cpu1_EMBED_FILELIST "CFE_PSP_CF_IMAGE,cf.tar")
