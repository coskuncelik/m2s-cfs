#!/usr/bin/env bash
# 09-run-cfe-arm-cm3-qemu.sh — run cFE + counter_app on an emulated Cortex-M3 (QEMU mps2-an385), step 09 (D32).
#
# Mission freertos-arm-cm3-qemu: the step 08 Cortex-M3 mission (config/cfe/freertos-arm-cm3) with the board
# boards/qemu-mps2-an385 (overlay m2s_board.cmake). Generic start-up, newlib stubs and traps from
# boards/common-cm3. QEMU loads the ELF (-kernel) and resets the CPU, which starts at the vector table.
#
# Checks:
#   1. QEMU's mps2-an385 memory map still matches board.ld (SSRAM1 at 0, PSRAM at 0x21000000, UART0);
#   2. build + install with cFE's strict flags; the step 08 image checks on this image (ELF/ARMv7-M Thumb-2,
#      entry, vector table, link map, no allocator or heap linked, no target object referencing malloc/free);
#   3. RUNS runs of RUN_SECONDS under QEMU (-icount shift=auto,sleep=on,align=on), each checked like script 06: ES loads COUNTER_APP, OPERATIONAL,
#      "counter_app initialized" once, counter 1..N without gap, every interval 1 s +/- TOLERANCE_MS, drift
#      within DRIFT_MS, no error line and no "M2S TRAP/FAULT/ASSERT/PANIC/STOP" line;
#   4. sources/ unchanged.
#
# Usage: ./scripts/09-run-cfe-arm-cm3-qemu.sh [--clean]
#        Environment: RUNS (default 5), RUN_SECONDS (20), TOLERANCE_MS (50), DRIFT_MS (10).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
# shellcheck source=lib/counter-check.sh
source "${SCRIPT_DIR}/lib/counter-check.sh"
cfe_sim_init freertos-arm-cm3-qemu freertos-arm-cm3
# After cfe_sim_init: sets the ARM binutils names that cfe_sim_init set for the host.
# shellcheck source=lib/cm3-image.sh
source "${SCRIPT_DIR}/lib/cm3-image.sh"
LOG_FILE="${LOG_DIR}/09-run-cfe-arm-cm3-qemu.log"
RUN_DIR="${LOG_DIR}/09-qemu-runs"
MAP_COPY="${LOG_DIR}/09-core-cpu1.map"
RUNS="${RUNS:-5}"
RUN_SECONDS="${RUN_SECONDS:-20}"
TOLERANCE_MS="${TOLERANCE_MS:-50}"
DRIFT_MS="${DRIFT_MS:-10}"

QEMU="qemu-system-arm"
QEMU_MACHINE="mps2-an385"
# Console on UART0 -> stdout; no monitor, no graphics. Run in EXE_DIR by run_once (counter-check.sh).
# Guest time from the instruction count, held to host real time (D32): in QEMU's default mode SysTick runs
# on the host clock and a late tick is never made up, so on a busy host FreeRTOS time fell 6-13 % behind
# (measured 2026-11-06); with these options one 20 s run had 3.0 ms max deviation and 0.1 ms drift.
QEMU_TIMING="-icount shift=auto,sleep=on,align=on"
RUN_CMD="${QEMU} -M ${QEMU_MACHINE} ${QEMU_TIMING} -nographic -monitor none -serial stdio -kernel core-cpu1"
MALLOC_ALLOWED='^$'                    # no target object may reference malloc/free
MALLOC_ALLOWED_TEXT="no object (none allowed on the target)"

DO_CLEAN=0

parse_args() {
    local arg
    for arg in "$@"; do
        case "${arg}" in
            --clean) DO_CLEAN=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean])" ;;
        esac
    done
    [[ "${RUNS}" =~ ^[1-9][0-9]*$ ]] || die "RUNS must be a positive integer (got '${RUNS}')"
    [[ "${RUN_SECONDS}" =~ ^[1-9][0-9]*$ && "${RUN_SECONDS}" -ge 5 ]] || die "RUN_SECONDS must be >= 5 (got '${RUN_SECONDS}')"
    [[ "${TOLERANCE_MS}" =~ ^[1-9][0-9]*$ ]] || die "TOLERANCE_MS must be a positive integer (got '${TOLERANCE_MS}')"
    [[ "${DRIFT_MS}" =~ ^[1-9][0-9]*$ ]] || die "DRIFT_MS must be a positive integer (got '${DRIFT_MS}')"
    [[ -n "${EPOCHREALTIME:-}" ]] || die "bash 5 or later needed (EPOCHREALTIME)"
}

check_qemu() {
    command -v "${QEMU}" >/dev/null 2>&1 || die "'${QEMU}' not found — run scripts/01-check-prerequisites.sh for the install command"
    "${QEMU}" -machine help | grep -qE "^${QEMU_MACHINE}[[:space:]]" || die "${QEMU} has no machine ${QEMU_MACHINE}"
    log "OK   $("${QEMU}" --version | head -1)"
}

# The QEMU memory map the board's linker script and console rely on (info mtree of an idle machine).
check_qemu_memory_map() {
    local mtree want
    mtree="$(printf 'info mtree -f\nquit\n' | timeout 20 "${QEMU}" -M "${QEMU_MACHINE}" -display none -S \
             -monitor stdio -serial none 2>&1)" || die "cannot query the ${QEMU_MACHINE} memory map"
    for want in \
        '0000000000000000-00000000003fffff \(prio 0, ram\): mps.ssram1' \
        '0000000021000000-0000000021ffffff \(prio 0, ram\): mps.ram' \
        '0000000040004000-0000000040004fff \(prio 0, i/o\): uart'; do
        grep -qE "${want}" <<<"${mtree}" || die "${QEMU_MACHINE} memory map changed: no '${want//\\/}' (boards/qemu-mps2-an385/board.ld)"
    done
    log "OK   ${QEMU_MACHINE} memory map: SSRAM1 4 MiB at 0x0 (code), PSRAM 16 MiB at 0x21000000 (data), UART0 at 0x40004000"
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 09-run-cfe-arm-cm3-qemu start (RUNS=${RUNS}, RUN_SECONDS=${RUN_SECONDS}, TOLERANCE_MS=${TOLERANCE_MS}, DRIFT_MS=${DRIFT_MS}) ==="

    cfe_sim_check_prerequisites
    check_cross_tools
    check_qemu
    check_qemu_memory_map
    local before after
    before="$(cfe_sim_sources_status)"

    cfe_sim_build "${DO_CLEAN}"
    check_image
    check_map
    check_no_heap
    cfe_sim_check_static_only
    run_counter_app

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 09-run-cfe-arm-cm3-qemu OK ==="
}

main "$@"
