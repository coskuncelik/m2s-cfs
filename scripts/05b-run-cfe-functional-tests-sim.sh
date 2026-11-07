#!/usr/bin/env bash
# 05b-run-cfe-functional-tests-sim.sh — run cFE's own functional tests (cfe_testcase + cfe_assert) on the
# FreeRTOS Linux simulator and compare every assertion with stock cFE on native Linux (step 05b, D29).
#
# Phase 1, native reference: stock cFS (pc-linux PSP, POSIX OSAL), sources/cFS/cfe/cmake/sample_defs copied to
#   the build directory with config/cfe/native-ft laid over it (apps: cfe_assert, sample_lib, sample_app,
#   cfe_testcase, loaded dynamically). Result: exe/cpu1/cf/cfe_test.log on the host disk.
# Phase 2, FreeRTOS: mission freertos-posix-sim-ft (scripts/lib/cfe-sim.sh; same apps linked statically, plus
#   ft_logdump, which prints /cf/cfe_test.log from the simulator's RAM file system to the console as
#   "FTLOG|" lines between BEGIN/END markers). Runs ROUNDS times.
#
# Checks:
#   1. both builds succeed; FreeRTOS image: no FreeRTOS heap, malloc/free only in the host POSIX port;
#   2. each run completes (summary line present) within RUN_TIMEOUT; the FreeRTOS log is rebuilt from the
#      console and its size equals the size ft_logdump reports;
#   3. every assertion (status + file:line, in order) is the same as in the native reference, except the
#      entries in KNOWN_DIFFS; every FreeRTOS round gives the same list. Inside the UNORDERED_GROUPS tests
#      (concurrent child tasks of equal priority) the order is free and only the set of assertions counts
#      (comparison code in scripts/lib/cfe-ft.sh, shared with script 09c);
#   4. sources/ unchanged.
#
# Host side effects of the native phase (stock pc-linux PSP, as script 02): SysV shared memory segments and
# files under the build directory.
#
# Usage: ./scripts/05b-run-cfe-functional-tests-sim.sh [--clean]   (env: ROUNDS default 3, RUN_TIMEOUT default 300)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
cfe_sim_init freertos-posix-sim-ft
# Native reference, log recovery and assertion comparison (shared with script 09c).
# shellcheck source=lib/cfe-ft.sh
source "${SCRIPT_DIR}/lib/cfe-ft.sh"
LOG_FILE="${LOG_DIR}/05b-run-cfe-functional-tests-sim.log"
RUN_DIR="${LOG_DIR}/05b-cfe-ft"
ROUNDS="${ROUNDS:-3}"
RUN_TIMEOUT="${RUN_TIMEOUT:-300}"

# Allowed differences from the native reference, as "<status> <file:line>" of the FreeRTOS assertion (each one
# backed by a decision in docs/DECISIONS.md).
#   D29: a child task's local variable is not inside the stack buffer given to it — on the FreeRTOS POSIX port a
#        task runs on its host thread's stack (same cause as D22). Simulator only.
KNOWN_DIFFS=(
    "FAIL es_task_test.c:108"
)

DO_CLEAN=0
FT_LABEL="FreeRTOS"
FT_RUN_CMD="./core-cpu1"

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

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 05b-run-cfe-functional-tests-sim start (ROUNDS=${ROUNDS}, RUN_TIMEOUT=${RUN_TIMEOUT}) ==="

    cfe_sim_check_prerequisites
    local before after
    before="$(cfe_sim_sources_status)"
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"

    native_reference
    cfe_sim_build "${DO_CLEAN}"
    cfe_sim_check_static_only
    ft_rounds

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 05b-run-cfe-functional-tests-sim OK ==="
}

main "$@"
