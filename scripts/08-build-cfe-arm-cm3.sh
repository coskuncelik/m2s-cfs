#!/usr/bin/env bash
# 08-build-cfe-arm-cm3.sh — cross-build cFE on FreeRTOS for ARM Cortex-M3 and check the image (step 08, D31).
#
# Build and link only; nothing runs (an emulator run is step 09, the board later). Mission
# config/cfe/freertos-arm-cm3: the same cFE, OSAL FreeRTOS layer, PSP sources and counter_app as the
# simulator, built with arm-none-eabi-gcc (newlib-nano) against the FreeRTOS GCC/ARM_CM3 port, with the
# OSAL BSP osal-ext/freertos-arm-cm3, the PSP psp-ext/fsw/freertos-arm-cm3 and the PLACEHOLDER board
# boards/link-check (vector table, linker script, newlib stubs with the _sbrk trap).
#
# Checks:
#   1. configure + build + install with cFE's strict flags (-Werror; project code also -Wextra);
#   2. core-cpu1 is an ELF32 ARM EABI5 image for an ARMv7-M (Cortex-M3) Thumb target; the entry point is
#      Reset_Handler; vector table entries 0 (initial stack), 1 (reset), 11 (SVCall), 14 (PendSV) and
#      15 (SysTick) point to _estack, Reset_Handler and the FreeRTOS port handlers;
#   3. the link map exists (copied to logs/);
#   4. no dynamic allocation: newlib's allocator objects are not linked; malloc/free/calloc/realloc, their
#      _r forms and _sbrk exist only as the board's traps (syscalls.c, per the link map); no FreeRTOS heap
#      or dynamic FreeRTOS object creation; no target object references malloc/free;
#   5. footprint report: sections, the 20 largest RAM symbols, comparison with the simulator build;
#   6. sources/ unchanged.
#
# Usage: ./scripts/08-build-cfe-arm-cm3.sh [--clean]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
cfe_sim_init freertos-arm-cm3 freertos-arm-cm3
# After cfe_sim_init: sets the ARM binutils names (NM, SIZE, ...) that cfe_sim_init set for the host.
# shellcheck source=lib/cm3-image.sh
source "${SCRIPT_DIR}/lib/cm3-image.sh"
LOG_FILE="${LOG_DIR}/08-build-cfe-arm-cm3.log"
REPORT="${LOG_DIR}/08-footprint.txt"
MAP_COPY="${LOG_DIR}/08-core-cpu1.map"

MALLOC_ALLOWED='^$'                    # no target object may reference malloc/free
MALLOC_ALLOWED_TEXT="no object (none allowed on the target)"
SIM_BIN="${ROOT_DIR}/build/cfe-freertos-sim/exe/cpu1/core-cpu1"

DO_CLEAN=0

parse_args() {
    local arg
    for arg in "$@"; do
        case "${arg}" in
            --clean) DO_CLEAN=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean])" ;;
        esac
    done
}

footprint_report() {
    local bin="${EXE_DIR}/core-cpu1" sim
    {
        echo "core-cpu1 (Cortex-M3, board link-check) — sections (arm-none-eabi-size -A):"
        "${SIZE}" -A "${bin}" | grep -vE '^\.(debug|comment|ARM\.attributes)|^$'
        echo
        echo "Totals (Berkeley): $("${SIZE}" "${bin}" | tail -1 | awk '{print "text=" $1 " data=" $2 " bss=" $3}')"
        echo
        echo "20 largest RAM symbols (data and bss), bytes:"
        "${NM}" -S --size-sort -t d "${bin}" | awk '$3 ~ /^[bBdD]$/' | tail -20 | sort -k2,2nr \
            | awk '{ printf "  %10d  %s\n", $2, $4 }'
        echo
        if [[ -x "${SIM_BIN}" ]]; then
            sim="$(size "${SIM_BIN}" | tail -1)"
            echo "Simulator core-cpu1 (x86-64, for comparison): $(awk '{print "text=" $1 " data=" $2 " bss=" $3}' <<<"${sim}")"
        else
            echo "Simulator core-cpu1 not built (run scripts/06 for the comparison)."
        fi
    } >"${REPORT}"
    log "OK   footprint report: ${REPORT}"
    sed 's/^/     /' "${REPORT}" | grep -E 'Totals|Simulator core-cpu1|^\s+[0-9]+\s' | head -8
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 08-build-cfe-arm-cm3 start ==="

    cfe_sim_check_prerequisites
    check_cross_tools
    local before after
    before="$(cfe_sim_sources_status)"

    cfe_sim_build "${DO_CLEAN}"
    check_image
    check_got
    check_map
    check_no_heap
    cfe_sim_check_static_only
    footprint_report

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 08-build-cfe-arm-cm3 OK ==="
}

main "$@"
