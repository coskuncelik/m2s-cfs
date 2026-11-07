#!/usr/bin/env bash
# scripts/lib/cfe-ft.sh — cFE functional tests (cfe_testcase + cfe_assert) against the stock native-Linux
# reference, shared by script 05b (FreeRTOS simulator) and script 09c (Cortex-M3 on QEMU). Sourced, not run,
# after scripts/lib/cfe-sim.sh and cfe_sim_init.
#
# The caller sets: RUN_DIR, ROUNDS, RUN_TIMEOUT, DO_CLEAN, KNOWN_DIFFS (allowed differences, may be empty),
# FT_LABEL (target name in messages), FT_RUN_CMD (command line run in EXE_DIR), optionally FT_CONSOLE_ERROR_RE;
# and provides log and die.

NATIVE_DIR="${ROOT_DIR}/build/cfe-native-ft"
NATIVE_OVERLAY="${ROOT_DIR}/config/cfe/native-ft"
NATIVE_EXE="${NATIVE_DIR}/exe/cpu1"
SUMMARY_RE='^\[ *END\] [0-9]+ SUMMARY'

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
    # stdin /dev/null (explicit; a background job of a script gets it anyway): QEMU's -serial stdio reads stdin.
    (cd "${dir}" && exec ${cmd}) </dev/null >"${out}" 2>&1 &
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

# Rebuilds /cf/cfe_test.log from a target console log; returns 1 if markers or size do not match. Carriage
# returns are removed first (a UART console sends CR LF; the log itself has none).
extract_ft_log() {
    local console="$1" out="$2" reported actual
    local text
    text="$(tr -d '\r' <"${console}")"
    grep -q '^FT_LOGDUMP: BEGIN ' <<<"${text}" || { echo "no BEGIN marker"; return 1; }
    reported="$(sed -nE 's/^FT_LOGDUMP: END ([0-9]+) bytes$/\1/p' <<<"${text}")"
    [[ -n "${reported}" ]] || { echo "no END marker (or a read error)"; return 1; }
    awk '/^FT_LOGDUMP: BEGIN /{f=1; next} /^FT_LOGDUMP: END /{f=0} f && /^FTLOG\|/{printf "%s\n", substr($0, 7); next}
         f && /^FTLOG\+/{printf "%s", substr($0, 7)}' <<<"${text}" >"${out}"
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

# Runs the target image ROUNDS times with FT_RUN_CMD (in EXE_DIR) and compares each round with the native
# reference. FT_LABEL names the target in messages; a console line matching FT_CONSOLE_ERROR_RE (if set) fails
# the round.
ft_rounds() {
    local r secs out log size failed=0 diffs bad
    for (( r = 1; r <= ROUNDS; r++ )); do
        out="${RUN_DIR}/freertos-round${r}-console.log"
        log="${RUN_DIR}/freertos-round${r}-cfe_test.log"
        if ! secs="$(run_until "${FT_RUN_CMD}" "${EXE_DIR}" "${out}" "${out}" '^FT_LOGDUMP: (END|cannot)|did not start')"; then
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
        if [[ -n "${FT_CONSOLE_ERROR_RE:-}" ]] && bad="$(tr -d '\r' <"${out}" | grep -E "${FT_CONSOLE_ERROR_RE}" | head -1)" && [[ -n "${bad}" ]]; then
            log "ERROR round ${r}: ${bad}"
            failed=$(( failed + 1 ))
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
    log "${FT_LABEL} rounds: $(( ROUNDS - failed ))/${ROUNDS} passed (logs: ${RUN_DIR})"
    (( failed == 0 )) || die "cFE functional tests on ${FT_LABEL} differ from the native reference"
}
