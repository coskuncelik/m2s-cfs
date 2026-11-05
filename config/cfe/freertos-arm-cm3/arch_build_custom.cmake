# m2s-cfs: compile options for all target code (Cortex-M3, step 08): the same as the simulator mission.
# -Wno-format-truncation / -Wno-stringop-truncation: gcc 13 flags stock cFE snprintf calls that upstream
# has reviewed as safe (e.g. cfe_evs_utils.c:549, "SAD" comment); cFE's sample_defs inhibits the same two
# warnings for its host build (mission_build_custom.cmake).
add_compile_options(
    -std=c99
    -pedantic
    -Wall
    -Wstrict-prototypes
    -Wwrite-strings
    -Wpointer-arith
    -Wcast-align
    -Werror
    -Wno-format-truncation
    -Wno-stringop-truncation
)

# Link executables with the library list in a GNU ld group (searched repeatedly until no new symbol is
# resolved). cFE links its core modules with --whole-archive only when a CPU has dynamically loaded apps
# (cmake/target/CMakeLists.txt); with statically linked apps only, an app that uses a core function the core
# itself does not reference (e.g. CFE_TBL_Load from cfe_testcase) would otherwise fail to link (D29).
# -u CFE_PSP_Main forces the PSP's start-up object into the image. OSAL's libosal_bsp also defines
# OS_Application_Startup (bsp_default_app_startup.c, a fallback for programs without one); without this the
# linker may take that fallback, and cFE never starts (seen with the functional-test mission, D29).
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> -Wl,-u,CFE_PSP_Main <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")

