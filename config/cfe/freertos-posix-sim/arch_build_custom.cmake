# m2s-cfs: compile options for all target code, as cFE's sample_defs (strict C99, warnings as errors).
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
