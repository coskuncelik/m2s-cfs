#!/usr/bin/env bash
# scripts/lib/counter-check.sh — counter_app run and output checks, shared by script 06 (project build) and
# script 07-check-bundle (exported bundle) and script 09 (QEMU Cortex-M3). Sourced, not run. The caller sets
# EXE_DIR (directory holding core-cpu1), RUN_DIR, RUNS, RUN_SECONDS, TOLERANCE_MS and DRIFT_MS, optionally
# RUN_CMD (command run in EXE_DIR; default ./core-cpu1) and RUN_WARMUP_SECONDS (default 0: one untimed launch
# of RUN_CMD before the counted runs, output discarded, so a cold start of the program or emulator is not
# counted), and provides log and die.

# Fixed strings that must appear in each run.
EXPECTED_LINES=(
    "CFE_ES_ParseFileEntry: Loading file: /cf/counter_app.so, APP: COUNTER_APP"
    "CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state"
    # counter_app events (EVS port 1 console format ".../<app> <event id>: ...") and file test (step 11a, D37).
    "/COUNTER_APP 1: counter_app initialized"
    "/COUNTER_APP 4: Read /cf/cfe_es_startup.scr: "
    "/COUNTER_APP 2: File test passed on /ram/counter: "
    "counter_app file test: PASS"
)
# None of these may appear in a run (extended regex, case-insensitive). "M2S ..." lines are the Cortex-M3
# trap, assert, fault, panic and stop messages (boards/common-cm3/m2s_cm3_diag.h).
ERROR_PATTERN='error|fail|panic|cannot|too small|abort|assert|could not|M2S (TRAP|FAULT|ASSERT|PANIC|STOP)'

# Runs RUN_CMD (default ./core-cpu1) once in EXE_DIR; writes "<host seconds.micros> <line>" per output line to
# $1 (a trailing CR, as sent by a UART console, is removed). Prints the exit status. Standard input is
# /dev/null: timeout runs the command in its own (background) process group, and a program that reads the
# terminal from there is stopped by SIGTTIN (QEMU's -serial stdio does; seen when script 09 ran in a terminal).
run_once() {
    local out="$1" rc
    set +e
    # RUN_CMD is a command line with arguments: word splitting is intended.
    # shellcheck disable=SC2086
    (cd "${EXE_DIR}" && timeout -s INT "${RUN_SECONDS}" ${RUN_CMD:-./core-cpu1} </dev/null) 2>&1 \
        | while IFS= read -r line; do printf '%s %s\n' "${EPOCHREALTIME}" "${line%$'\r'}"; done >"${out}"
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

    echo "${count} counter lines, last $( (( count > 0 )) && echo "${values[count-1]}" || echo none), max interval deviation $(( max_dev / 1000 )).$(printf '%03d' $(( max_dev % 1000 ))) ms, drift ${drift} us over $(( count > 1 ? count - 1 : 0 )) s"
    return $(( problems > 0 ))
}

# Runs core-cpu1 RUNS times; logs each result; dies if any run fails.
run_counter_app() {
    local r rc out summary failed=0
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"
    if (( ${RUN_WARMUP_SECONDS:-0} > 0 )); then
        # Same stdin rule as run_once. Its output and exit status do not count.
        # shellcheck disable=SC2086
        (cd "${EXE_DIR}" && timeout -s INT "${RUN_WARMUP_SECONDS}" ${RUN_CMD:-./core-cpu1} </dev/null) >/dev/null 2>&1 || true
        log "     warm-up launch (${RUN_WARMUP_SECONDS} s, not counted)"
    fi
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
