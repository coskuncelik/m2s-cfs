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
cfe_sim_init freertos-posix-sim
LOG_FILE="${LOG_DIR}/06-run-counter-app-sim.log"
RUN_DIR="${LOG_DIR}/06-counter-app-runs"
RUNS="${RUNS:-5}"
RUN_SECONDS="${RUN_SECONDS:-20}"
TOLERANCE_MS="${TOLERANCE_MS:-50}"
DRIFT_MS="${DRIFT_MS:-10}"

# Fixed strings that must appear in each run.
EXPECTED_LINES=(
    "CFE_ES_ParseFileEntry: Loading file: /cf/counter_app.so, APP: COUNTER_APP"
    "CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state"
)
# None of these may appear in a run (extended regex, case-insensitive).
ERROR_PATTERN='error|fail|panic|cannot|too small|abort|assert|could not'

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

# Runs core-cpu1 once; writes "<host seconds.micros> <line>" per output line to $1. Prints the exit status.
run_once() {
    local out="$1" rc
    set +e
    (cd "${EXE_DIR}" && timeout -s INT "${RUN_SECONDS}" ./core-cpu1) 2>&1 \
        | while IFS= read -r line; do printf '%s %s\n' "${EPOCHREALTIME}" "${line}"; done >"${out}"
    rc="${PIPESTATUS[0]}"
    set -e
    echo "${rc}"
}

# Checks one stamped run log; prints a summary and returns non-zero on failure.
check_run() {
    local out="$1" rc="$2" line problems=0 inits errors count
    local -a stamps=() values=()

    (( rc == 124 )) || { echo "exit ${rc} (124 expected)"; problems=$(( problems + 1 )); }
    for line in "${EXPECTED_LINES[@]}"; do
        grep -qF -- "${line}" "${out}" || { echo "missing line: ${line}"; problems=$(( problems + 1 )); }
    done
    inits="$(grep -cE '^[0-9.]+ counter_app initialized$' "${out}" || true)"
    (( inits == 1 )) || { echo "\"counter_app initialized\" ${inits} times (1 expected)"; problems=$(( problems + 1 )); }
    errors="$(cut -d' ' -f2- "${out}" | grep -ciE -- "${ERROR_PATTERN}" || true)"
    (( errors == 0 )) || { echo "${errors} error line(s)"; problems=$(( problems + 1 )); }

    mapfile -t stamps < <(sed -nE 's/^([0-9.]+) counter_app counter: [0-9]+$/\1/p' "${out}")
    mapfile -t values < <(sed -nE 's/^[0-9.]+ counter_app counter: ([0-9]+)$/\1/p' "${out}")
    count="${#values[@]}"
    (( count >= RUN_SECONDS - 2 )) || { echo "${count} counter lines (at least $(( RUN_SECONDS - 2 )) expected)"; problems=$(( problems + 1 )); }

    # Sequence 1..count and spacing, in integer microseconds (no bc).
    local i us first_us=0 prev_us dt dev max_dev=0 bad_seq=0 bad_dt=0 span drift=0
    for (( i = 0; i < count; i++ )); do
        (( values[i] == i + 1 )) || bad_seq=$(( bad_seq + 1 ))
        us=$(( 10#${stamps[i]%.*} * 1000000 + 10#${stamps[i]#*.} ))
        if (( i > 0 )); then
            dt=$(( us - prev_us ))
            dev=$(( dt > 1000000 ? dt - 1000000 : 1000000 - dt ))
            (( dev > max_dev )) && max_dev="${dev}"
            (( dev <= TOLERANCE_MS * 1000 )) || bad_dt=$(( bad_dt + 1 ))
        fi
        (( i == 0 )) && first_us="${us}"
        prev_us="${us}"
    done
    if (( count > 1 )); then
        span=$(( prev_us - first_us ))
        drift=$(( span - (count - 1) * 1000000 ))
        (( (drift < 0 ? -drift : drift) <= DRIFT_MS * 1000 )) \
            || { echo "drift ${drift} us over $(( count - 1 )) s (limit ${DRIFT_MS} ms)"; problems=$(( problems + 1 )); }
    fi
    (( bad_seq == 0 )) || { echo "${bad_seq} counter value(s) out of sequence"; problems=$(( problems + 1 )); }
    (( bad_dt == 0 )) || { echo "${bad_dt} interval(s) outside 1 s +/- ${TOLERANCE_MS} ms"; problems=$(( problems + 1 )); }

    echo "${count} counter lines, last ${values[count-1]:-none}, max interval deviation $(( max_dev / 1000 )).$(printf '%03d' $(( max_dev % 1000 ))) ms, drift ${drift} us over $(( count > 1 ? count - 1 : 0 )) s"
    return $(( problems > 0 ))
}

run_counter_app() {
    local r rc out summary failed=0
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"
    for (( r = 1; r <= RUNS; r++ )); do
        out="${RUN_DIR}/run${r}.log"
        rc="$(run_once "${out}")"
        if summary="$(check_run "${out}" "${rc}")"; then
            log "OK   run ${r}: ${summary}"
        else
            failed=$(( failed + 1 ))
            log "ERROR run ${r}:"
            printf '%s\n' "${summary}" | sed 's/^/        /'
        fi
    done
    log "counter_app runs: $(( RUNS - failed ))/${RUNS} passed (logs: ${RUN_DIR})"
    (( failed == 0 )) || die "counter_app runs failed"
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
