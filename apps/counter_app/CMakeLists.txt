######################################################################
#
# m2s-cfs: counter_app — platform-independent cFE app: prints a message at start-up and a counter
# every second through the OSAL console (OS_printf, D28). Same source for the simulator and the board.
# Linked statically into the cFE executable (<cpu>_STATIC_APPLIST; the dynamic loader is off).
#
######################################################################
project(M2S_COUNTER_APP C)

add_cfe_app(counter_app fsw/src/counter_app.c)

# Project code: strict warnings.
target_compile_options(counter_app PRIVATE -Wall -Wextra -Werror)
