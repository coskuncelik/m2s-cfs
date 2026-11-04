# m2s-cfs PSP module: cFE timebase from the FreeRTOS tick (shared by the simulator and the board).
add_psp_module(timebase_freertos cfe_psp_timebase_freertos.c)

# FreeRTOS headers come with the kernel target built by the OSAL BSP.
target_include_directories(timebase_freertos PRIVATE
    $<TARGET_PROPERTY:m2s_freertos_kernel,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_options(timebase_freertos PRIVATE -Wall -Wextra -Werror)
