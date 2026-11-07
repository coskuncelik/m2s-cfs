#!/usr/bin/env bash
# 09c-run-cfe-functional-tests-arm-cm3-qemu.sh — cFE's own functional tests (cfe_testcase + cfe_assert) on an
# emulated Cortex-M3 (QEMU mps2-an385), compared assertion by assertion with stock cFE on native Linux
# (step 09c, D34).
#
# Phase 1, native reference: as script 05b (scripts/lib/cfe-ft.sh).
# Phase 2, Cortex-M3: mission freertos-arm-cm3-qemu-ft (overlay on config/cfe/freertos-arm-cm3; a copy of the
#   simulator's functional-test mission config/cfe/freertos-posix-sim-ft with cpu1_SYSTEM freertos-arm-cm3, plus
#   m2s_board.cmake: board qemu-mps2-an385 and the test-only allocator, because cfe_assert uses ut_assert,
#   which allocates with malloc). ft_logdump prints /cf/cfe_test.log on the UART; QEMU runs ROUNDS times.
#
# Checks:
#   1. the mission's copied files are identical to the simulator's functional-test mission (targets.cmake:
#      every non-comment line except cpu1_SYSTEM);
#   2. both builds succeed; Cortex-M3 image: ELF/ARMv7-M Thumb-2, entry, vectors, no .got outside .data, test
#      allocator linked (not the traps), no newlib allocator, no FreeRTOS heap or dynamic FreeRTOS objects;
#   3. each round (QEMU default timing, no -icount) completes within RUN_TIMEOUT; the log is rebuilt from the console with the reported size;
#      no "M2S TRAP/FAULT/ASSERT/PANIC/STOP" line on the console;
#   4. every assertion (status + file:line, in order; as a set inside the multi-task SB tests) equals the native
#      reference, with NO allowed differences (D29's simulator-only es_task_test.c:108 must pass here); every
#      round gives the same list;
#   5. sources/ unchanged.
#
# Usage: ./scripts/09c-run-cfe-functional-tests-arm-cm3-qemu.sh [--clean]
#        (env: ROUNDS default 3, RUN_TIMEOUT default 600 seconds per round; a round takes about 2 min)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
cfe_sim_init freertos-arm-cm3-qemu-ft freertos-arm-cm3
# After cfe_sim_init: ARM binutils names (cm3-image.sh) and the shared functional-test code (cfe-ft.sh).
# shellcheck source=lib/cm3-image.sh
source "${SCRIPT_DIR}/lib/cm3-image.sh"
# shellcheck source=lib/cfe-ft.sh
source "${SCRIPT_DIR}/lib/cfe-ft.sh"
LOG_FILE="${LOG_DIR}/09c-run-cfe-functional-tests-arm-cm3-qemu.log"
RUN_DIR="${LOG_DIR}/09c-cfe-ft"
MAP_COPY="${LOG_DIR}/09c-core-cpu1.map"
ROUNDS="${ROUNDS:-3}"
RUN_TIMEOUT="${RUN_TIMEOUT:-600}"

SIM_FT_DEFS="${ROOT_DIR}/config/cfe/freertos-posix-sim-ft"
KNOWN_DIFFS=()   # none on the Cortex-M3 (D34)
FT_LABEL="Cortex-M3 (QEMU)"
QEMU="qemu-system-arm"
QEMU_MACHINE="mps2-an385"
# QEMU's default timing (no -icount, D34): these tests check results, not timing precision (that is step 09's
# job, which keeps -icount). With -icount one round took about 25 min (log dump over the polled UART about
# 19 min) instead of about 2 min. Semihosting on for a clean exit path (D33).
FT_RUN_CMD="${QEMU} -M ${QEMU_MACHINE} -semihosting-config enable=on,target=native -nographic -monitor none -serial stdio -kernel core-cpu1"
FT_CONSOLE_ERROR_RE='^M2S (TRAP|FAULT|ASSERT|PANIC|STOP)'
DO_CLEAN=0

parse_args() {
    local arg
    for arg in "$@"; do
        case "${arg}" in
            --clean) DO_CLEAN=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean])" ;;
        esac
    done
    [[ "${ROUNDS}" =~ ^[1-9][0-9]*$ ]] || die "ROUNDS must be a positive integer (got '${ROUNDS}')"
    [[ "${RUN_TIMEOUT}" =~ ^[1-9][0-9]*$ ]] || die "RUN_TIMEOUT must be a positive integer (got '${RUN_TIMEOUT}')"
}

check_qemu() {
    command -v "${QEMU}" >/dev/null 2>&1 || die "'${QEMU}' not found — run scripts/01-check-prerequisites.sh for the install command"
    "${QEMU}" -machine help | grep -qE "^${QEMU_MACHINE}[[:space:]]" || die "${QEMU} has no machine ${QEMU_MACHINE}"
    log "OK   $("${QEMU}" --version | head -1)"
}

# The Cortex-M3 functional-test mission is a copy of the simulator's; they must not drift apart.
check_mission_copy() {
    local f
    for f in cpu1_cfe_es_startup.scr cf_files.txt arch_build_extra.cmake; do
        cmp -s "${SIM_FT_DEFS}/${f}" "${OVERLAY_DEFS}/${f}" || die "${OVERLAY_DEFS}/${f} differs from ${SIM_FT_DEFS}/${f}"
    done
    diff <(grep -vE '^\s*(#|$)|^SET\(cpu1_SYSTEM ' "${SIM_FT_DEFS}/targets.cmake") \
         <(grep -vE '^\s*(#|$)|^SET\(cpu1_SYSTEM ' "${OVERLAY_DEFS}/targets.cmake") >/dev/null \
        || die "${OVERLAY_DEFS}/targets.cmake differs from the simulator's beyond cpu1_SYSTEM"
    log "OK   mission freertos-arm-cm3-qemu-ft matches the simulator's functional-test mission (except cpu1_SYSTEM)"
}

# Test image: the test-only allocator, never the traps, never newlib's allocator.
check_test_image() {
    local syms
    syms="$("${NM}" "${EXE_DIR}/core-cpu1")"
    grep -Eq ' T M2S_TestHeap_Report$' <<<"${syms}" || die "test image without the test allocator (test_heap.c)"
    if grep -Eq ' [BbDd] M2S_AllocTrapWhich$' <<<"${syms}"; then die "test image contains the allocator traps"; fi
    if grep -Eq 'lib_a-(nano-)?(mallocr|freer|callocr|reallocr|malignr|msizer|sbrkr)\.o' "${MAP_COPY}"; then
        die "newlib allocator objects in the link map"
    fi
    if grep -Eq '\b(pvPortMalloc|vPortFree|xTaskCreate|xQueueGenericCreate|xQueueCreateMutex|xQueueCreateCountingSemaphore|xTimerCreate|xEventGroupCreate)$' <<<"${syms}"; then
        die "FreeRTOS heap or dynamic FreeRTOS object creation symbol found"
    fi
    log "OK   test image: test allocator only (test_heap.c), no newlib allocator, no FreeRTOS heap or dynamic objects"
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 09c-run-cfe-functional-tests-arm-cm3-qemu start (ROUNDS=${ROUNDS}, RUN_TIMEOUT=${RUN_TIMEOUT}) ==="

    cfe_sim_check_prerequisites
    check_cross_tools
    check_qemu
    check_mission_copy
    local before after
    before="$(cfe_sim_sources_status)"
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"

    native_reference
    cfe_sim_build "${DO_CLEAN}"
    check_image
    check_got
    check_map
    check_test_image
    ft_rounds

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 09c-run-cfe-functional-tests-arm-cm3-qemu OK ==="
}

main "$@"
