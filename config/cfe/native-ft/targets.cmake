######################################################################
#
# m2s-cfs: native (stock pc-linux) reference for cFE's functional tests (step 05b).
# Laid over a build-time copy of sources/cFS/cfe/cmake/sample_defs by scripts/05b; replaces its
# targets.cmake. Same app set as the FreeRTOS test mission: cfe_assert (added by cFE's
# mission_defaults.cmake), sample_lib and sample_app (the tests share SAMPLE_APP.ExampleTable) and
# cfe_testcase, loaded dynamically as in stock cFS.
#
######################################################################

SET(MISSION_NAME "SampleMission")
SET(SPACECRAFT_ID 0x42)

list(APPEND MISSION_GLOBAL_APPLIST sample_lib sample_app cfe_testcase)

SET(FT_INSTALL_SUBDIR "host/functional-test")

SET(MISSION_CPUNAMES cpu1)

SET(cpu1_PROCESSORID 1)
SET(cpu1_APPLIST)
SET(cpu1_FILELIST cfe_es_startup.scr)
SET(cpu1_SYSTEM i686-linux-gnu)
