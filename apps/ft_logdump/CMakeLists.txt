######################################################################
#
# m2s-cfs: ft_logdump — test-only cFE app for the functional-test mission (step 05b, D29).
# Waits until cFE's functional test app (CFE_TEST_APP) has run and exited, then prints its log file
# /cf/cfe_test.log to the console between BEGIN/END markers, so the complete results leave the
# simulator's RAM file system. Not part of any flight mission.
#
######################################################################
project(M2S_FT_LOGDUMP C)

add_cfe_app(ft_logdump fsw/src/ft_logdump.c)

target_compile_options(ft_logdump PRIVATE -Wall -Wextra -Werror)
