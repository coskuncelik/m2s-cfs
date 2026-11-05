#!/usr/bin/env bash
# 05-build-cfe-freertos-sim.sh — build stock cFE (core-cpu1) on the m2s-cfs FreeRTOS OSAL layer with the
# project PSP "freertos-posix-sim", and run it on the Linux simulator (step 05). Checks the cFE core only;
# the apps in the mission (counter_app since step 06) are checked by script 06. Build and static-memory
# steps are shared with script 06 (scripts/lib/cfe-sim.sh, which lists the parts used).
#
# Checks:
#   1. configure + build + install (cFE's strict flags: -std=c99 -pedantic -Werror; project code also -Wextra);
#   2. no FreeRTOS heap or dynamic FreeRTOS object creation in core-cpu1; malloc/free referenced only by the
#      host POSIX port (port.c, wait_for_event.c);
#   3. core-cpu1 runs RUNS times for RUN_SECONDS each, must still be running at the end (timeout, 124),
#      and its log must show every expected start-up line and no error line;
#   4. sources/cFS and sources/FreeRTOS-Kernel unchanged.
#
# Usage: ./scripts/05-build-cfe-freertos-sim.sh [--clean]      (env: RUNS default 5, RUN_SECONDS default 20)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
cfe_sim_init
LOG_FILE="${LOG_DIR}/05-build-cfe-freertos-sim.log"
RUN_DIR="${LOG_DIR}/05-cfe-runs"
RUNS="${RUNS:-5}"
RUN_SECONDS="${RUN_SECONDS:-20}"

# Every one of these must appear in each run log (fixed strings).
EXPECTED_LINES=(
    "CFE_PSP: embedded startup script installed as /cf/cfe_es_startup.scr"
    "CFE_PSP: using the FreeRTOS tick (1000 Hz) as CFE timebase"
    "POWER ON RESET due to Power Cycle"
    "cFE EVS Initialized"
    "cFE SB Initialized"
    "cFE ES Initialized"
    "cFE TIME Initialized"
    "cFE TBL Initialized"
    "CFE_ES_StartApplications: Opened ES App Startup file: /cf/cfe_es_startup.scr"
    "CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state"
    "CFE_TIME 21: Stop FLYWHEEL"
)
# None of these may appear in a run log (extended regex, case-insensitive).
ERROR_PATTERN='error|fail|panic|cannot|too small|abort|assert'

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
    [[ "${RUN_SECONDS}" =~ ^[1-9][0-9]*$ ]] || die "RUN_SECONDS must be a positive integer (got '${RUN_SECONDS}')"
}

run_cfe() {
    local r rc out line missing errors failed=0
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"
    for (( r = 1; r <= RUNS; r++ )); do
        out="${RUN_DIR}/run${r}.log"
        rc=0
        (cd "${EXE_DIR}" && timeout -s INT "${RUN_SECONDS}" ./core-cpu1) >"${out}" 2>&1 || rc=$?
        missing=0
        for line in "${EXPECTED_LINES[@]}"; do
            if ! grep -qF -- "${line}" "${out}"; then
                log "ERROR run ${r}: missing line: ${line}"
                missing=$(( missing + 1 ))
            fi
        done
        errors="$(grep -ciE -- "${ERROR_PATTERN}" "${out}" || true)"
        if (( rc == 124 && missing == 0 && errors == 0 )); then
            log "OK   run ${r}: still running after ${RUN_SECONDS}s, all ${#EXPECTED_LINES[@]} expected lines, no error lines ($(wc -l <"${out}") lines)"
        else
            failed=$(( failed + 1 ))
            log "ERROR run ${r}: exit ${rc} (124 expected), ${missing} missing line(s), ${errors} error line(s)"
            grep -iE -- "${ERROR_PATTERN}" "${out}" | head -5 | sed 's/^/        /' || true
        fi
    done
    log "cFE runs: $(( RUNS - failed ))/${RUNS} passed (logs: ${RUN_DIR})"
    (( failed == 0 )) || die "cFE runs failed"
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 05-build-cfe-freertos-sim start (RUNS=${RUNS}, RUN_SECONDS=${RUN_SECONDS}) ==="

    cfe_sim_check_prerequisites
    local before after
    before="$(cfe_sim_sources_status)"

    cfe_sim_build "${DO_CLEAN}"
    cfe_sim_check_static_only
    run_cfe

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 05-build-cfe-freertos-sim OK ==="
}

main "$@"
