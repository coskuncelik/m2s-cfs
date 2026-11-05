##########################################################################
#
# cfs-m2s-freertos: OSAL configuration for the FreeRTOS Linux simulator.
# Included after default_osconfig.cmake, so these values win.
#
##########################################################################

# Not implemented by the FreeRTOS layer (OSAL's portable "no-*" stubs are used).
set(OSAL_CONFIG_INCLUDE_NETWORK         FALSE)
set(OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER  FALSE)
set(OSAL_CONFIG_INCLUDE_STATIC_LOADER   TRUE)

# Priorities are enforced by FreeRTOS, so never fall back to permissive mode.
set(OSAL_CONFIG_DEBUG_PERMISSIVE_MODE   FALSE)

# OSAL debug messages off (default_osconfig.cmake turns them on for the native build).
set(OSAL_CONFIG_DEBUG_PRINTF            FALSE)
