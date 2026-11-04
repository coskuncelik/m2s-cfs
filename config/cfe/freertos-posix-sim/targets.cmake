######################################################################
#
# m2s-cfs: cFE mission targets — one CPU on the FreeRTOS Linux simulator (step 05).
# Passed as MISSION_DEFS by scripts/05-build-cfe-freertos-sim.sh. No applications yet.
#
######################################################################

SET(MISSION_NAME "M2SMission")
SET(SPACECRAFT_ID 0x42)

SET(MISSION_CPUNAMES cpu1)

SET(cpu1_PROCESSORID 1)
SET(cpu1_SYSTEM freertos-posix-sim)   # toolchain-freertos-posix-sim.cmake
SET(cpu1_APPLIST)                     # none in step 05
SET(cpu1_FILELIST)

# Startup script embedded in the executable; the PSP writes it to /cf at start-up (D5).
# Available as CFE_ES_STARTUP_SCR_DATA[] / CFE_ES_STARTUP_SCR_SIZE; source: cpu1_cfe_es_startup.scr.
SET(cpu1_EMBED_FILELIST "CFE_ES_STARTUP_SCR,cfe_es_startup.scr")
