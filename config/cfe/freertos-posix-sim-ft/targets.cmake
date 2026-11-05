######################################################################
#
# m2s-cfs: cFE functional-test mission on the FreeRTOS Linux simulator (step 05b, D29).
# Laid over config/cfe/freertos-posix-sim by scripts/lib/cfe-sim.sh (replaces its targets.cmake,
# startup script and cf_files.txt; adds arch_build_extra.cmake). Same app set as the native reference
# (config/cfe/native-ft): cfe_assert, sample_lib, sample_app (its table is shared by the tests),
# cfe_testcase; plus the test-only ft_logdump. All linked statically.
#
######################################################################

SET(MISSION_NAME "M2SMissionFT")
SET(SPACECRAFT_ID 0x42)

# cFE adds cfe_assert to the dynamic app list of every mission (mission_defaults.cmake); here it is static.
list(REMOVE_ITEM MISSION_GLOBAL_APPLIST cfe_assert)

SET(MISSION_CPUNAMES cpu1)

SET(cpu1_PROCESSORID 1)
SET(cpu1_SYSTEM freertos-posix-sim)
SET(cpu1_APPLIST)
SET(cpu1_FILELIST)

SET(cpu1_STATIC_APPLIST cfe_assert sample_lib sample_app cfe_testcase ft_logdump)
# Module name = cFE name in the startup script (4th field).
SET(cpu1_STATIC_SYMLIST
    "CFE_Assert_LibInit,ASSERT_LIB"
    "SAMPLE_LIB_Init,SAMPLE_LIB"
    "SAMPLE_APP_Main,SAMPLE_APP"
    "CFE_TestMain,CFE_TEST_APP"
    "FT_LOGDUMP_Main,FT_LOGDUMP"
)

SET(cpu1_EMBED_FILELIST "CFE_PSP_CF_IMAGE,cf.tar")
