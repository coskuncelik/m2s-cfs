#!/usr/bin/env bash
# 12c-run-ground-station-sim.sh — end-to-end test of the ground station (tools/ground_station) with cFS on the
# Linux simulator (step 12c, D43).
#
#   1. byte check: the ground station's command packets are byte-identical to cFE's cmdUtil for the same
#      commands (noop, set-period 2000, tlm-enable, es-noop), both sent to a local check port;
#   2. builds the simulator mission (script 06's build), starts it (console lines stamped with host time), starts
#      "ground_station.py listen --log", then sends with "ground_station.py send":
#        tlm-enable; noop; set-period 2000; set-period 100 (below the limit); raw 0x1890 9 (unknown code);
#        raw 0x1890 0 00000000 (wrong length); reset; set-period 1000; es-noop
#      and checks every effect in the decoded telemetry (counter_app events 10-15, housekeeping command and error
#      counters and period, ES event 3), the counter lines 2 s apart on the console while the period is 2000 ms,
#      and no error line on the console except counter_app's expected error events 13, 14 and 15.
#
# Usage: ./scripts/12c-run-ground-station-sim.sh [--clean]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
# shellcheck source=lib/counter-check.sh
source "${SCRIPT_DIR}/lib/counter-check.sh"
cfe_sim_init freertos-posix-sim
LOG_FILE="${LOG_DIR}/12c-run-ground-station-sim.log"
RUN_DIR="${LOG_DIR}/12c-ground-station"
GS="${ROOT_DIR}/tools/ground_station/ground_station.py"
CMD_PORT=1234
TLM_PORT=1235
CHECK_PORT=47001
SIM_PID=""
GS_PID=""
STAMP_PID=""
SENT_LOG=""

# Background jobs of a non-interactive shell start with SIGINT ignored: only core-cpu1 (which installs its own
# handler) is stopped with SIGINT; the listener and the console stamper get SIGTERM.
cleanup() {
    if [[ -n "${SIM_PID}" ]]; then
        kill -INT "${SIM_PID}" 2>/dev/null || true
        wait "${SIM_PID}" 2>/dev/null || true
    fi
    local p
    for p in "${GS_PID}" "${STAMP_PID}"; do
        if [[ -n "${p}" ]]; then
            kill -TERM "${p}" 2>/dev/null || true
            wait "${p}" 2>/dev/null || true
        fi
    done
}

port_free() {
    python3 -I -c 'import socket,sys; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.bind(("127.0.0.1",int(sys.argv[1])))' "$1" 2>/dev/null
}

wait_for_line() { # $1 file, $2 fixed string, $3 seconds
    local i
    for (( i = 0; i < $3 * 10; i++ )); do
        grep -qF -- "$2" "$1" 2>/dev/null && return 0
        sleep 0.1
    done
    return 1
}

# Sends one ground station command and records "<host time> <command>" in SENT_LOG.
gs_send() {
    printf '%s %s\n' "${EPOCHREALTIME}" "$*" >>"${SENT_LOG}"
    log "     send $*"
    python3 -I "${GS}" --cmd-port "${CMD_PORT}" send "$@" || die "ground_station.py send $* failed"
}

# Byte check against cmdUtil: each tool sends the same commands to a local check port; the captures must match.
byte_check() {
    local cmdutil="${BUILD_DIR}/exe/host/cmdUtil" out="${RUN_DIR}/bytecheck.txt"
    port_free "${CHECK_PORT}" || die "UDP port ${CHECK_PORT} is in use"
    python3 -I -c '
import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(("127.0.0.1", int(sys.argv[1]))); s.settimeout(10)
print("READY", flush=True)
for _ in range(8):
    print(s.recvfrom(65536)[0].hex(), flush=True)' "${CHECK_PORT}" >"${out}" &
    local cap=$!
    wait_for_line "${out}" READY 5 || die "byte check listener did not start"
    "${cmdutil}" --port="${CHECK_PORT}" --endian=LE --pktid=0x1890 --cmdcode=0 >/dev/null
    "${cmdutil}" --port="${CHECK_PORT}" --endian=LE --pktid=0x1890 --cmdcode=2 --uint32=2000 >/dev/null
    "${cmdutil}" --port="${CHECK_PORT}" --endian=LE --pktid=0x1880 --cmdcode=6 --string=16:127.0.0.1 >/dev/null
    "${cmdutil}" --port="${CHECK_PORT}" --endian=LE --pktid=0x1806 --cmdcode=0 >/dev/null
    python3 -I "${GS}" --cmd-port "${CHECK_PORT}" send noop
    python3 -I "${GS}" --cmd-port "${CHECK_PORT}" send set-period 2000
    python3 -I "${GS}" --cmd-port "${CHECK_PORT}" send tlm-enable
    python3 -I "${GS}" --cmd-port "${CHECK_PORT}" send es-noop
    wait "${cap}" || die "byte check listener did not receive 8 packets (see ${out})"
    mapfile -t pk < <(grep -v READY "${out}")
    local i
    for i in 0 1 2 3; do
        [[ "${pk[i]}" == "${pk[i+4]}" ]] || die "byte check: cmdUtil ${pk[i]} != ground station ${pk[i+4]}"
    done
    log "OK   byte check: ground station packets identical to cmdUtil (noop ${pk[0]}, set-period 2000 ${pk[1]}, tlm-enable, es-noop ${pk[3]})"
}

main() {
    local arg do_clean=0 console tlm before after fifo
    for arg in "$@"; do
        case "${arg}" in
            --clean) do_clean=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean])" ;;
        esac
    done
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    trap cleanup EXIT
    log "=== 12c-run-ground-station-sim start ==="
    cfe_sim_check_prerequisites
    command -v python3 >/dev/null 2>&1 || die "'python3' not found"
    [[ -f "${GS}" ]] || die "missing ${GS}"
    before="$(cfe_sim_sources_status)"
    cfe_sim_build "${do_clean}"
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"
    console="${RUN_DIR}/console.log"
    tlm="${RUN_DIR}/telemetry.log"
    SENT_LOG="${RUN_DIR}/sent.log"
    : >"${SENT_LOG}"

    byte_check
    port_free "${CMD_PORT}" || die "UDP port ${CMD_PORT} is in use"
    port_free "${TLM_PORT}" || die "UDP port ${TLM_PORT} is in use"

    python3 -I "${GS}" --tlm-port "${TLM_PORT}" listen --log "${tlm}" >/dev/null 2>&1 &
    GS_PID=$!
    wait_for_line "${tlm}" "LISTEN" 5 || die "ground station listener did not start"

    fifo="${RUN_DIR}/console.fifo"
    mkfifo "${fifo}"
    while IFS= read -r line; do printf '%s %s\n' "${EPOCHREALTIME}" "${line}"; done <"${fifo}" >"${console}" &
    STAMP_PID=$!
    (cd "${EXE_DIR}" && exec ./core-cpu1 </dev/null >"${fifo}" 2>&1) &
    SIM_PID=$!
    wait_for_line "${console}" "entering OPERATIONAL state" 15 || die "cFS did not reach OPERATIONAL (see ${console})"
    wait_for_line "${console}" "CI_LAB listening on UDP port: ${CMD_PORT}" 5 || die "ci_lab is not listening"
    log "OK   cFS operational, ground station listening"

    gs_send tlm-enable;              sleep 2.5
    gs_send noop;                    sleep 2
    gs_send set-period 2000;         sleep 9
    gs_send set-period 100;          sleep 1.5
    gs_send raw 0x1890 9;            sleep 1.5
    gs_send raw 0x1890 0 00000000;   sleep 1.5
    gs_send reset;                   sleep 2
    gs_send set-period 1000;         sleep 3
    gs_send es-noop;                 sleep 2

    kill -INT "${SIM_PID}" 2>/dev/null || true
    wait "${SIM_PID}" 2>/dev/null || true
    SIM_PID=""
    wait "${STAMP_PID}" 2>/dev/null || true
    STAMP_PID=""
    kill -TERM "${GS_PID}" 2>/dev/null || true
    wait "${GS_PID}" 2>/dev/null || true
    GS_PID=""

    check_results "${console}" "${tlm}"

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 12c-run-ground-station-sim OK (logs: ${RUN_DIR}) ==="
}

# Host time at which command $1 (as written to SENT_LOG) was sent.
sent_at() { awk -v c="$1" '{t = $1; $1 = ""; sub(/^ /, ""); if ($0 == c) { print t; exit }}' "${SENT_LOG}"; }

# Fails unless the telemetry log has a line after time $1 (and before $2) matching extended regex $3.
expect_tlm() { # $1 from, $2 to, $3 regex, $4 description
    awk -v a="$1" -v b="$2" -v re="$3" '$1 + 0 > a + 0 && $1 + 0 < b + 0 && $0 ~ re { found = 1 } END { exit !found }' "$5" \
        || die "telemetry: no ${4}"
    log "OK   telemetry: ${4}"
}

check_results() {
    local console="$1" tlm="$2" t_en t_noop t_p2000 t_p100 t_cc t_len t_reset t_p1000 t_es end bad intervals
    t_en="$(sent_at tlm-enable)"; t_noop="$(sent_at noop)"; t_p2000="$(sent_at 'set-period 2000')"
    t_p100="$(sent_at 'set-period 100')"; t_cc="$(sent_at 'raw 0x1890 9')"; t_len="$(sent_at 'raw 0x1890 0 00000000')"
    t_reset="$(sent_at reset)"; t_p1000="$(sent_at 'set-period 1000')"; t_es="$(sent_at es-noop)"; end=9999999999

    expect_tlm "${t_en}" "${t_noop}" "COUNTER_HK .* cmdcnt=0 errcnt=0" "housekeeping after tlm-enable (counters 0)" "${tlm}"
    expect_tlm "${t_noop}" "${t_p2000}" "EVENT COUNTER_APP 10 INFO: NOOP command received" "NOOP event (10)" "${tlm}"
    expect_tlm "${t_noop}" "${t_p2000}" "COUNTER_HK .* period=1000 .*cmdcnt=1 errcnt=0" "HK after NOOP (command counter 1)" "${tlm}"
    expect_tlm "${t_p2000}" "${t_p100}" "EVENT COUNTER_APP 12 INFO: Period set to 2000 ms" "SET_PERIOD 2000 event (12)" "${tlm}"
    expect_tlm "${t_p2000}" "${t_p100}" "COUNTER_HK .* period=2000 .*cmdcnt=2 errcnt=0" "HK period 2000, command counter 2" "${tlm}"
    expect_tlm "${t_p100}" "${t_cc}" "EVENT COUNTER_APP 13 ERROR: SET_PERIOD 100 ms rejected: limits 500..5000 ms" "SET_PERIOD 100 rejected (13)" "${tlm}"
    expect_tlm "${t_p100}" "${t_cc}" "COUNTER_HK .* period=2000 .*cmdcnt=2 errcnt=1" "HK error counter 1, period still 2000" "${tlm}"
    expect_tlm "${t_cc}" "${t_len}" "EVENT COUNTER_APP 14 ERROR: Unknown command code 9" "unknown code event (14)" "${tlm}"
    expect_tlm "${t_cc}" "${t_len}" "COUNTER_HK .* errcnt=2" "HK error counter 2" "${tlm}"
    expect_tlm "${t_len}" "${t_reset}" "EVENT COUNTER_APP 15 ERROR: Command length error: code 0, length 12, expected 8" "length error event (15)" "${tlm}"
    expect_tlm "${t_len}" "${t_reset}" "COUNTER_HK .* cmdcnt=2 errcnt=3" "HK error counter 3" "${tlm}"
    expect_tlm "${t_reset}" "${t_p1000}" "EVENT COUNTER_APP 11 INFO: Command counters reset" "reset event (11)" "${tlm}"
    expect_tlm "${t_reset}" "${t_p1000}" "COUNTER_HK .* cmdcnt=0 errcnt=0" "HK counters 0 after reset" "${tlm}"
    expect_tlm "${t_p1000}" "${end}" "COUNTER_HK .* period=1000 .*cmdcnt=1 errcnt=0" "HK period back to 1000 (command counter 1)" "${tlm}"
    expect_tlm "${t_es}" "${end}" "EVENT CFE_ES 3 INFO: No-op command" "ES NOOP event (CFE_ES 3)" "${tlm}"

    # Console: counter lines 2 s apart while the period is 2000 ms (from 2.5 s after the command to the next one).
    intervals="$(awk -v a="${t_p2000}" -v b="${t_p100}" '$2 == "counter_app" && $3 == "counter:" && $1 > a + 2.5 && $1 < b {
            if (p) { d = ($1 - p) * 1000; n++; if (d < 1950 || d > 2050) bad++; printf "%.0f ", d } p = $1 }
            END { printf "| %d %d\n", n + 0, bad + 0 }' "${console}")"
    read -r n bad <<<"${intervals##*|}"
    (( n >= 2 && bad == 0 )) || die "console: counter intervals with period 2000: ${intervals}"
    log "OK   console: counter intervals at period 2000 ms: ${intervals%%|*}ms"

    # Console: no error line except counter_app's expected error events 13, 14, 15.
    bad="$(cut -d' ' -f2- "${console}" | grep -iE -- "${ERROR_PATTERN}" | grep -vE '/COUNTER_APP 1[345]: ' || true)"
    [[ -z "${bad}" ]] || { printf '%s\n' "${bad}" | head -5; die "unexpected error line(s) on the console"; }
    log "OK   console: no error line besides counter_app events 13, 14, 15"
}

main "$@"
