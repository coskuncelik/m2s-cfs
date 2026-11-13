######################################################################
#
# m2s-cfs: counter_app — platform-independent cFE app: prints a message at start-up and a counter
# every second through the OSAL console (OS_printf, D28). Same source for the simulator and the board.
# Linked statically into the cFE executable (<cpu>_STATIC_APPLIST; the dynamic loader is off).
#
######################################################################
project(M2S_COUNTER_APP C)

add_cfe_app(counter_app fsw/src/counter_app.c fsw/src/counter_app_cmds.c)

# Public headers (message IDs, command and telemetry formats, table format), also used by other apps' tables.
target_include_directories(counter_app PUBLIC fsw/inc)

# Uses NASA's sample_lib: public header and link order; the mission must also list sample_lib in
# its static app list and load it with a CFE_LIB line before counter_app in the startup script.
add_cfe_app_dependency(counter_app sample_lib)

# Project code: strict warnings.
target_compile_options(counter_app PRIVATE -Wall -Wextra -Werror)
