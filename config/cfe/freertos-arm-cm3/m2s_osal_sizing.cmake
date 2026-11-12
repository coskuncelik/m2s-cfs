# m2s-cfs: OSAL limits of the Cortex-M3 flight configuration (step 10, D36). Included by
# freertos-arm-cm3_osconfig.cmake after config/osal/freertos-arm-cm3/osal_config.cmake (which the OSAL test
# build, script 09b, uses unchanged). Measured with scripts/10-memory-report.sh (counter_app and the cFE
# functional tests): 9-10 tasks, 5-6 queues, 17 of 20 mutexes in use.
set(OSAL_CONFIG_MAX_TASKS           24)   # 9 in use; cFE needs OS_MAX_TASKS >= its own tasks + apps
set(OSAL_CONFIG_MAX_QUEUES          16)   # 5 in use (one per SB pipe); CFE_PLATFORM_SB_MAX_PIPES <= this
set(OSAL_CONFIG_MAX_NUM_OPEN_FILES  16)   # 0 open at the end of the run
set(OSAL_CONFIG_PRINTF_BUFFER_DEPTH 64)   # console lines buffered for the console task (was 100)
