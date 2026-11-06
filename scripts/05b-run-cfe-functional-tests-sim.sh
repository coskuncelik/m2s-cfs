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
#      (concurrent child tasks of equal priority) the order is free and only the set of assertions counts;
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
LOG_FILE="${LOG_DIR}/05b-run-cfe-functional-tests-sim.log"
RUN_DIR="${LOG_DIR}/05b-cfe-ft"
ROUNDS="${ROUNDS:-3}"
RUN_TIMEOUT="${RUN_TIMEOUT:-300}"

NATIVE_DIR="${ROOT_DIR}/build/cfe-native-ft"
NATIVE_OVERLAY="${ROOT_DIR}/config/cfe/native-ft"
NATIVE_EXE="${NATIVE_DIR}/exe/cpu1"

# Allowed differences from the native reference, as "<status> <file:line>" of the FreeRTOS assertion (each one
# backed by a decision in docs/DECISIONS.md).
#   D29: a child task's local variable is not inside the stack buffer given to it — on the FreeRTOS POSIX port a
#        task runs on its host thread's stack (same cause as D22). Simulator only.
KNOWN_DIFFS=(
    "FAIL es_task_test.c:108"
)

SUMMARY_RE='^\[ *END\] [0-9]+ SUMMARY'
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

# Test groups (UtTest_Add names) whose assertions come from concurrent child tasks of equal priority. Their
# relative order depends on where a time slice ends, on any OS (sb_performance_test.c: TestBulkTransferMulti2
# starts CmdXmit and TlmXmit at priority 150; TestBulkTransferMulti4 starts two pairs). Inside these groups the
# assertions are compared as a set (same lines, same results, any order); everywhere else the order counts (D29).
UNORDERED_GROUPS=(
    "2 Thread Bulk Transfer"
    "4 Thread Bulk Transfer"
)

# Assertion list of a cfe_assert log: "<STATUS> <file:line>" per assertion, in order (INFO/BEGIN/END omitted).
# Inside an UNORDERED_GROUPS group the group's lines are sorted, so only their set is compared.
assertions() {
    local groups
    groups="$(printf '%s\n' "${UNORDERED_GROUPS[@]}")"
    awk -v groups="${groups}" '
        BEGIN { n = split(groups, g, "\n"); for (i = 1; i <= n; i++) if (g[i] != "") unordered[g[i]] = 1 }
        function flush(   i, j, t) {
            for (i = 2; i <= cnt; i++) {            # insertion sort (portable awk, small groups)
                t = buf[i]
                for (j = i - 1; j >= 1 && buf[j] > t; j--) buf[j + 1] = buf[j]
                buf[j + 1] = t
            }
            for (i = 1; i <= cnt; i++) print buf[i]
            cnt = 0; inset = 0
        }
        /^\[BEGIN\] [0-9]+ / { name = $0; sub(/^\[BEGIN\] [0-9]+ /, "", name); inset = (name in unordered); cnt = 0; next }
        /^\[ *END\] [0-9]+ / { if (inset) flush(); next }
        match($0, /^\[ *(PASS|FAIL|TSF|TTF|MIR|N\/A|ABORT|WARN)\] [0-9.]+ [^ ]+:[0-9]+ /) {
            line = substr($0, 1, RLENGTH - 1)
            st = line; sub(/^\[ */, "", st); sub(/\].*/, "", st)
            loc = line; sub(/.* /, "", loc)
            if (inset) buf[++cnt] = st " " loc; else print st " " loc
        }
        END { if (inset) flush() }
    ' "$1"
}

# Starts $1 (command) in directory $2 with output to $3; waits until file $4 matches regex $5 or RUN_TIMEOUT,
# then stops it with SIGINT. Prints the number of seconds taken; returns 1 on timeout.
run_until() {
    local cmd="$1" dir="$2" out="$3" watch="$4" regex="$5" pid t=0
    (cd "${dir}" && exec ${cmd}) >"${out}" 2>&1 &
    pid=$!
    while kill -0 "${pid}" 2>/dev/null && (( t < RUN_TIMEOUT )); do
        if [[ -f "${watch}" ]] && grep -qE "${regex}" "${watch}"; then
            break
        fi
        sleep 1
        t=$(( t + 1 ))
    done
    sleep 1
    kill -INT "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
    echo "${t}"
    [[ -f "${watch}" ]] && grep -qE "${regex}" "${watch}"
}

native_reference() {
    local defs="${NATIVE_DIR}/defs" secs
    if (( DO_CLEAN )); then
        log "removing ${NATIVE_DIR}"
        rm -rf "${NATIVE_DIR}"
    fi
    rm -rf "${defs}"
    mkdir -p "${defs}"
    cp -a "${CFS_DIR}/cfe/cmake/sample_defs/." "${defs}/"
    cp -a "${NATIVE_OVERLAY}/." "${defs}/"
    if [[ ! -f "${NATIVE_DIR}/work/CMakeCache.txt" ]]; then
        log "native: configuring (stock pc-linux, SIMULATION=native)"
        cmake -S "${CFS_DIR}/cfe" -B "${NATIVE_DIR}/work" \
            -DMISSION_SOURCE_DIR="${CFS_DIR}" \
            -DMISSIONCONFIG=sample \
            -DMISSION_DEFS="${defs}" \
            -DSIMULATION=native \
            -DCMAKE_INSTALL_PREFIX=/exe
    fi
    log "native: building"
    make -C "${NATIVE_DIR}/work" -j"$(nproc)" mission-all
    make -C "${NATIVE_DIR}/work" DESTDIR="${NATIVE_DIR}" mission-install
    [[ -x "${NATIVE_EXE}/core-cpu1" ]] || die "native core-cpu1 not produced"

    rm -f "${NATIVE_EXE}/cf/cfe_test.log"
    if ! secs="$(run_until "./core-cpu1 -R PO" "${NATIVE_EXE}" "${RUN_DIR}/native-console.log" \
                 "${NATIVE_EXE}/cf/cfe_test.log" "${SUMMARY_RE}")"; then
        die "native: no test summary within ${RUN_TIMEOUT}s (see ${RUN_DIR}/native-console.log)"
    fi
    cp "${NATIVE_EXE}/cf/cfe_test.log" "${RUN_DIR}/native-cfe_test.log"
    assertions "${RUN_DIR}/native-cfe_test.log" >"${RUN_DIR}/native.assertions"
    log "OK   native reference (${secs}s): $(grep -E "${SUMMARY_RE}" "${RUN_DIR}/native-cfe_test.log" | sed 's/  */ /g')"
}

# Rebuilds /cf/cfe_test.log from a FreeRTOS console log; returns 1 if markers or size do not match.
extract_ft_log() {
    local console="$1" out="$2" reported actual
    grep -q '^FT_LOGDUMP: BEGIN ' "${console}" || { echo "no BEGIN marker"; return 1; }
    reported="$(sed -nE 's/^FT_LOGDUMP: END ([0-9]+) bytes$/\1/p' "${console}")"
    [[ -n "${reported}" ]] || { echo "no END marker (or a read error)"; return 1; }
    awk '/^FT_LOGDUMP: BEGIN /{f=1; next} /^FT_LOGDUMP: END /{f=0} f && /^FTLOG\|/{printf "%s\n", substr($0, 7); next}
         f && /^FTLOG\+/{printf "%s", substr($0, 7)}' "${console}" >"${out}"
    actual="$(wc -c <"${out}")"
    [[ "${actual}" == "${reported}" ]] || { echo "rebuilt ${actual} bytes, ft_logdump reported ${reported}"; return 1; }
    echo "${actual} bytes"
}

# Compares a round's assertions with the native reference; prints differences not in KNOWN_DIFFS.
compare_with_native() {
    local ft="$1" line known entry unexpected=0
    while IFS= read -r line; do
        known=0
        for entry in "${KNOWN_DIFFS[@]}"; do
            # The FreeRTOS line must be exactly the entry; the native line it replaces must be at the same place.
            if [[ "${line}" == "> ${entry}" || ( "${line}" == "< "* && "${line##* }" == "${entry##* }" ) ]]; then
                known=1
                break
            fi
        done
        if (( known == 0 )); then
            echo "        unexpected: ${line}"
            unexpected=$(( unexpected + 1 ))
        fi
    done < <(diff "${RUN_DIR}/native.assertions" "${ft}" | grep -E '^[<>] ' || true)
    return $(( unexpected > 0 ))
}

freertos_rounds() {
    local r secs out log size failed=0 diffs
    for (( r = 1; r <= ROUNDS; r++ )); do
        out="${RUN_DIR}/freertos-round${r}-console.log"
        log="${RUN_DIR}/freertos-round${r}-cfe_test.log"
        if ! secs="$(run_until "./core-cpu1" "${EXE_DIR}" "${out}" "${out}" '^FT_LOGDUMP: (END|cannot)|did not start')"; then
            log "ERROR round ${r}: no log dump within ${RUN_TIMEOUT}s (see ${out})"
            failed=$(( failed + 1 ))
            continue
        fi
        if ! size="$(extract_ft_log "${out}" "${log}")"; then
            log "ERROR round ${r}: test log not recovered: ${size}"
            failed=$(( failed + 1 ))
            continue
        fi
        assertions "${log}" >"${RUN_DIR}/freertos-round${r}.assertions"
        if ! grep -qE "${SUMMARY_RE}" "${log}"; then
            log "ERROR round ${r}: no summary line in the test log"
            failed=$(( failed + 1 ))
            continue
        fi
        if (( r > 1 )) && ! cmp -s "${RUN_DIR}/freertos-round1.assertions" "${RUN_DIR}/freertos-round${r}.assertions"; then
            log "ERROR round ${r}: assertion list differs from round 1"
            failed=$(( failed + 1 ))
        fi
        if diffs="$(compare_with_native "${RUN_DIR}/freertos-round${r}.assertions")"; then
            log "OK   round ${r} (${secs}s, log ${size}): $(grep -E "${SUMMARY_RE}" "${log}" | sed 's/  */ /g'); same as native except ${#KNOWN_DIFFS[@]} known"
        else
            log "ERROR round ${r} (${secs}s): $(grep -E "${SUMMARY_RE}" "${log}" | sed 's/  */ /g'); differences from native:"
            printf '%s\n' "${diffs}"
            failed=$(( failed + 1 ))
        fi
    done
    log "FreeRTOS rounds: $(( ROUNDS - failed ))/${ROUNDS} passed (logs: ${RUN_DIR})"
    (( failed == 0 )) || die "cFE functional tests on FreeRTOS differ from the native reference"
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
    freertos_rounds

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 05b-run-cfe-functional-tests-sim OK ==="
}

main "$@"
