#!/usr/bin/env bash
# 06-run-counter-app-sim.sh — build cFE with counter_app for the FreeRTOS Linux simulator and check the app's
# output (step 06). Build and static-memory steps are shared with script 05 (scripts/lib/cfe-sim.sh).
#
# counter_app (apps/counter_app) is linked statically and started from the embedded startup script; it prints
# through OS_printf (D28). Each output line is stamped with the host clock ($EPOCHREALTIME) as it arrives.
#
# Checks:
#   1. build + install (counter_app with -Wall -Wextra -Werror), no FreeRTOS heap, malloc/free only in the host
#      POSIX port;
#   2. core-cpu1 runs RUNS times for RUN_SECONDS each and must still be running at the end (timeout, 124);
#   3. each run: ES loads COUNTER_APP and reaches OPERATIONAL, "counter_app initialized" exactly once,
#      counter lines 1, 2, 3 ... with no gap or repeat, at least RUN_SECONDS - 2 of them, consecutive counter
#      lines 1 s +/- TOLERANCE_MS apart (host clock), first-to-last span within DRIFT_MS of whole seconds
#      (catches a slow drift that single intervals hide), and no error line;
#   4. sources/cFS and sources/FreeRTOS-Kernel unchanged.
#
# Usage: ./scripts/06-run-counter-app-sim.sh [--clean]
#        (env: RUNS default 5, RUN_SECONDS default 20, TOLERANCE_MS default 50, DRIFT_MS default 10)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
# shellcheck source=lib/counter-check.sh
source "${SCRIPT_DIR}/lib/counter-check.sh"
cfe_sim_init freertos-posix-sim
LOG_FILE="${LOG_DIR}/06-run-counter-app-sim.log"
RUN_DIR="${LOG_DIR}/06-counter-app-runs"
RUNS="${RUNS:-5}"
RUN_SECONDS="${RUN_SECONDS:-20}"
TOLERANCE_MS="${TOLERANCE_MS:-50}"
DRIFT_MS="${DRIFT_MS:-10}"

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

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 06-run-counter-app-sim start (RUNS=${RUNS}, RUN_SECONDS=${RUN_SECONDS}, TOLERANCE_MS=${TOLERANCE_MS}, DRIFT_MS=${DRIFT_MS}) ==="

    cfe_sim_check_prerequisites
    local before after
    before="$(cfe_sim_sources_status)"

    cfe_sim_build "${DO_CLEAN}"
    cfe_sim_check_static_only
    run_counter_app

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 06-run-counter-app-sim OK ==="
}

main "$@"
