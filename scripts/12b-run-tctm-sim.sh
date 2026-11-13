#!/usr/bin/env bash
# 12b-run-tctm-sim.sh — commands in and telemetry out over UDP on the Linux simulator (step 12b, D42).
#
# Builds the simulator mission (config/cfe/freertos-posix-sim: counter_app, sample_lib, sch_lab, ci_lab, to_lab;
# same build as script 06), starts it, captures UDP telemetry on 127.0.0.1:TLM_PORT with scripts/lib/udp-capture.py
# and sends commands to ci_lab (127.0.0.1:CMD_PORT) with cFE's cmdUtil:
#   t=0 .. ENABLE_AFTER s  no command: no telemetry may arrive (to_lab output is off);
#   then                    to_lab OUTPUT_ENABLE (MID 0x1880, code 6, dest_IP "127.0.0.1");
#   NOOP_AFTER s later      counter_app NOOP (MID 0x1890, code 0).
# Checks: telemetry only after the enable; counter_app housekeeping (MID 0x0890) at about 1 Hz with the command
# counter 0 before the NOOP and 1 after it, the counter rising and the period 1000 ms; at least one EVS event
# message; on the console the NOOP event and no error line (pattern of scripts/lib/counter-check.sh); sources/
# unchanged. Reports the simulator's CPU use (host) over the run.
#
# Usage: ./scripts/12b-run-tctm-sim.sh [--clean]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
# shellcheck source=lib/counter-check.sh
source "${SCRIPT_DIR}/lib/counter-check.sh"
cfe_sim_init freertos-posix-sim
LOG_FILE="${LOG_DIR}/12b-run-tctm-sim.log"
RUN_DIR="${LOG_DIR}/12b-tctm"
CMD_PORT=1234
TLM_PORT=1235
ENABLE_AFTER=4
NOOP_AFTER=3
AFTER_NOOP=5
CAPTURE="${SCRIPT_DIR}/lib/udp-capture.py"
CMDUTIL=""
SIM_PID=""
CAP_PID=""

# Stops what is still running. Waits only for these processes: a bare "wait" would also wait for the tee of the
# log (process substitution), which ends only when this script exits.
cleanup() {
    if [[ -n "${SIM_PID}" ]]; then
        kill -INT "${SIM_PID}" 2>/dev/null || true
        wait "${SIM_PID}" 2>/dev/null || true
    fi
    if [[ -n "${CAP_PID}" ]]; then
        kill "${CAP_PID}" 2>/dev/null || true
        wait "${CAP_PID}" 2>/dev/null || true
    fi
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

send_cmd() { # cmdUtil arguments; logs the command
    log "     cmdUtil $*"
    "${CMDUTIL}" --host=127.0.0.1 --port="${CMD_PORT}" --endian=LE "$@" >>"${RUN_DIR}/cmdutil.log" 2>&1 \
        || die "cmdUtil failed: $*"
}

cpu_ticks() { # utime + stime of pid $1 in clock ticks
    awk '{print $14 + $15}' "/proc/$1/stat"
}

main() {
    local arg do_clean=0 console capture t_enable t_noop t0 cpu0 cpu1 hz wall_s
    for arg in "$@"; do
        case "${arg}" in
            --clean) do_clean=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean])" ;;
        esac
    done
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    trap cleanup EXIT
    log "=== 12b-run-tctm-sim start ==="
    cfe_sim_check_prerequisites
    command -v python3 >/dev/null 2>&1 || die "'python3' not found"
    local before after
    before="$(cfe_sim_sources_status)"

    cfe_sim_build "${do_clean}"
    CMDUTIL="${BUILD_DIR}/exe/host/cmdUtil"
    [[ -x "${CMDUTIL}" ]] || die "cmdUtil not built at ${CMDUTIL}"
    port_free "${CMD_PORT}" || die "UDP port ${CMD_PORT} is in use on this host"
    port_free "${TLM_PORT}" || die "UDP port ${TLM_PORT} is in use on this host"

    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"
    console="${RUN_DIR}/console.log"
    capture="${RUN_DIR}/capture.log"

    python3 -I "${CAPTURE}" --port "${TLM_PORT}" --seconds $(( ENABLE_AFTER + NOOP_AFTER + AFTER_NOOP + 15 )) >"${capture}" 2>&1 &
    CAP_PID=$!
    wait_for_line "${capture}" "CAPTURE START" 5 || die "telemetry capture did not start: $(cat "${capture}")"

    (cd "${EXE_DIR}" && exec ./core-cpu1 </dev/null >"${console}" 2>&1) &
    SIM_PID=$!
    wait_for_line "${console}" "entering OPERATIONAL state" 15 || die "cFS did not reach OPERATIONAL (see ${console})"
    wait_for_line "${console}" "CI_LAB listening on UDP port: ${CMD_PORT}" 5 || die "ci_lab is not listening (see ${console})"
    t0="${EPOCHREALTIME}"
    cpu0="$(cpu_ticks "${SIM_PID}")"
    log "OK   cFS operational (pid ${SIM_PID}); ci_lab listening on ${CMD_PORT}"

    sleep "${ENABLE_AFTER}"
    t_enable="${EPOCHREALTIME}"
    send_cmd --pktid=0x1880 --cmdcode=6 --string=16:127.0.0.1
    sleep "${NOOP_AFTER}"
    t_noop="${EPOCHREALTIME}"
    send_cmd --pktid=0x1890 --cmdcode=0
    sleep "${AFTER_NOOP}"
    cpu1="$(cpu_ticks "${SIM_PID}")"
    wall_s="$(awk -v a="${t0}" -v b="${EPOCHREALTIME}" 'BEGIN {printf "%.1f", b - a}')"
    hz="$(getconf CLK_TCK)"
    kill -INT "${SIM_PID}" 2>/dev/null || true
    wait "${SIM_PID}" 2>/dev/null || true
    SIM_PID=""
    kill "${CAP_PID}" 2>/dev/null || true
    wait "${CAP_PID}" 2>/dev/null || true
    CAP_PID=""
    log "     simulator CPU over ${wall_s} s: $(awk -v t=$(( cpu1 - cpu0 )) -v hz="${hz}" -v w="${wall_s}" \
        'BEGIN {printf "%.2f s (%.1f %% of one core)", t / hz, 100 * t / hz / w}')"

    check_results "${console}" "${capture}" "${t_enable}" "${t_noop}"

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 12b-run-tctm-sim OK (logs: ${RUN_DIR}) ==="
}

# $1 console, $2 capture, $3 enable time, $4 NOOP time (host epoch seconds)
check_results() {
    local console="$1" capture="$2" t_enable="$3" t_noop="$4" errors summary
    errors="$(grep -ciE -- "${ERROR_PATTERN}" "${console}" || true)"
    (( errors == 0 )) || { grep -iE -- "${ERROR_PATTERN}" "${console}" | head -5; die "${errors} error line(s) on the console"; }
    grep -qF "/COUNTER_APP 10: NOOP command received" "${console}" || die "no NOOP event on the console"
    log "OK   console: NOOP event, no error line"

    summary="$(awk -v te="${t_enable}" -v tn="${t_noop}" '
        $1 ~ /^[0-9.]+$/ && $2 ~ /^MID=/ {
            t = $1 + 0; mid = $2
            if (t < te) { early++ }
            if (mid == "MID=0x0890") {
                for (i = 3; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
                hk++; if (hk == 1) { first = t; hdr = v["HDR"] } last = t
                if (v["period"] != 1000) bad_period++
                if (hk > 1 && v["counter"] + 0 <= prev) bad_counter++
                prev = v["counter"] + 0
                if (t < tn && v["cmdcnt"] != 0) bad_before++
                if (t > tn + 1.5) { after_noop++; if (v["cmdcnt"] != 1) bad_after++ }
            }
            if (index($0, " EVENT ") > 0) events++
        }
        END {
            rate = (hk > 1) ? (hk - 1) / (last - first) : 0
            printf "%d %d %.2f %d %d %d %d %d %d %s\n", early + 0, hk + 0, rate, bad_period + 0, bad_counter + 0, \
                bad_before + 0, after_noop + 0, bad_after + 0, events + 0, (hdr == "" ? "-" : hdr)
        }' "${capture}")"
    read -r early hk rate bad_period bad_counter bad_before after_noop bad_after events hdr <<<"${summary}"
    log "     capture: ${early} packet(s) before the enable, ${hk} counter_app HK packets (${rate}/s, header ${hdr} bytes), ${events} event message(s)"
    (( early == 0 )) || die "telemetry arrived before to_lab's OUTPUT_ENABLE"
    (( hk >= 6 )) || die "too few counter_app HK packets (${hk})"
    awk -v r="${rate}" 'BEGIN {exit !(r > 0.8 && r < 1.2)}' || die "HK rate ${rate}/s (about 1/s expected)"
    (( bad_period == 0 )) || die "${bad_period} HK packet(s) with a period other than 1000 ms"
    (( bad_counter == 0 )) || die "${bad_counter} HK packet(s) with a counter that did not rise"
    (( bad_before == 0 )) || die "${bad_before} HK packet(s) before the NOOP with a command counter other than 0"
    (( after_noop >= 2 && bad_after == 0 )) || die "after the NOOP: ${after_noop} HK packet(s), ${bad_after} with a command counter other than 1"
    (( events >= 1 )) || die "no EVS event message in the telemetry"
    grep -q "EVENT COUNTER_APP 10: NOOP command received" "${capture}" || die "the NOOP event did not arrive as telemetry"
    log "OK   telemetry: none before the enable; HK at ${rate}/s, command counter 0 -> 1 after the NOOP, counter rising, period 1000 ms; NOOP event received as telemetry"
}

main "$@"
