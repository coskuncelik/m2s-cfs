##########################################################################
#
# m2s-cfs: OSAL configuration for the FreeRTOS Linux simulator
# (passed as OSAL_CONFIGURATION_FILE; overrides sources/cFS/osal/default_config.cmake)
#
##########################################################################

# Not implemented by the FreeRTOS layer (OSAL's portable "no-*" stubs are used).
set(OSAL_CONFIG_INCLUDE_NETWORK         TRUE)   # host sockets (step 12a, D41)
set(OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER  FALSE)
set(OSAL_CONFIG_INCLUDE_STATIC_LOADER   TRUE)

# Priorities are enforced by FreeRTOS, so never fall back to permissive mode.
set(OSAL_CONFIG_DEBUG_PERMISSIVE_MODE   FALSE)
