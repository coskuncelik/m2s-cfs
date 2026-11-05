# cfs-m2s-freertos: compile options for all target code, as cFE's sample_defs (strict C99, warnings as errors).
# -Wno-format-truncation / -Wno-stringop-truncation: gcc 13 flags stock cFE snprintf calls that upstream
# has reviewed as safe (e.g. cfe_evs_utils.c:549, "SAD" comment).
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
# (cfe/cmake/target/CMakeLists.txt); with statically linked apps only, an app that uses a core function the
# core itself does not reference (e.g. CFE_TBL_Load) would otherwise fail to link.
# -u CFE_PSP_Main forces the PSP's start-up object into the image. OSAL's libosal_bsp also defines
# OS_Application_Startup (bsp_default_app_startup.c, a fallback for programs without one); without this the
# linker may take that fallback, and cFE never starts.
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> -Wl,-u,CFE_PSP_Main <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")
