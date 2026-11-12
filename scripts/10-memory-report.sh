#!/usr/bin/env bash
# 10-memory-report.sh — RAM use of the Cortex-M3 flight image, measured (step 10 phase 1, memory sizing).
#
# Builds the flight mission for QEMU (freertos-arm-cm3-qemu: config/cfe/freertos-arm-cm3 + boards/qemu-mps2-an385)
# a second time with -g in its own build directory (build/cfe-freertos-arm-cm3-qemu-mem), so gdb knows the types.
# -g adds debug sections only; script 10 checks that data and bss are the same size as the image built by
# script 09 (when present). Changes nothing in the configuration and nothing in sources/.
#
# Report (logs/10-memory-report.txt):
#   1. static RAM by owner (cFE ES/EVS/SB/TBL/TIME/..., OSAL shared, OSAL FreeRTOS layer, OSAL BSP, PSP,
#      FreeRTOS, board, newlib, main stack) from the link map, and the largest RAM symbols (nm);
#   2. measured use after RUN_SECONDS of counter_app under QEMU, read through QEMU's gdb stub with
#      scripts/lib/mem-probe-gdb.py: every task stack's high-water mark (0xA5 fill), the main stack, cFE ES memory
#      pools (SB and TBL buffers: peak = TailPosition), SB peaks, CDS, reset area, OSAL objects in use, OSAL
#      queues created, PSP reserved-memory layout.
# The counter_app run itself is checked only for start-up and trap lines (timing is script 09's job).
#
# --ft: the same measurement on script 09c's functional-test mission (freertos-arm-cm3-qemu-ft: the flight
#   configuration plus the cFE test apps and the test-only allocator), built with -g in
#   build/cfe-freertos-arm-cm3-qemu-ft-mem and probed after the tests and the log dump have finished
#   ("FT_LOGDUMP: END", within FT_TIMEOUT). The tests exercise cFE's command and API paths, so the core tasks'
#   stack peaks include them. Report: logs/10-memory-report-ft.txt. Data/bss compared with script 09c's image.
#
# Usage: ./scripts/10-memory-report.sh [--clean] [--ft]
#        Environment: RUN_SECONDS (default 20), FT_TIMEOUT (default 600), GDB_PORT (default 23410).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"

RUN_SECONDS="${RUN_SECONDS:-20}"
FT_TIMEOUT="${FT_TIMEOUT:-600}"
GDB_PORT="${GDB_PORT:-23410}"
QEMU="qemu-system-arm"
GDB="gdb-multiarch"
PROBE="${SCRIPT_DIR}/lib/mem-probe-gdb.py"
DO_CLEAN=0
FT=0
QEMU_PID=""

parse_args() {
    local arg
    for arg in "$@"; do
        case "${arg}" in
            --clean) DO_CLEAN=1 ;;
            --ft) FT=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean] [--ft])" ;;
        esac
    done
    [[ "${RUN_SECONDS}" =~ ^[1-9][0-9]*$ && "${RUN_SECONDS}" -ge 10 ]] || die "RUN_SECONDS must be >= 10 (got '${RUN_SECONDS}')"
    [[ "${FT_TIMEOUT}" =~ ^[1-9][0-9]*$ ]] || die "FT_TIMEOUT must be a positive integer (got '${FT_TIMEOUT}')"
    [[ "${GDB_PORT}" =~ ^[1-9][0-9]{3,4}$ ]] || die "GDB_PORT must be a port number (got '${GDB_PORT}')"
}
parse_args "$@"

if (( FT )); then
    cfe_sim_init freertos-arm-cm3-qemu-ft freertos-arm-cm3
    SUFFIX="-ft"
    REF_SCRIPT="09c"
    # As script 09c: QEMU default timing, semihosting for the test image's exit path.
    QEMU_OPTS="-semihosting-config enable=on,target=native"
else
    cfe_sim_init freertos-arm-cm3-qemu freertos-arm-cm3
    SUFFIX=""
    REF_SCRIPT="09"
    QEMU_OPTS="-icount shift=auto,sleep=on,align=on"
fi
# shellcheck source=lib/cm3-image.sh
source "${SCRIPT_DIR}/lib/cm3-image.sh"

# Own build directory: the -g build must not replace script 09's / 09c's image.
FLIGHT_EXE="${EXE_DIR}/core-cpu1"
BUILD_DIR="${BUILD_DIR}-mem"
MISSION_DEFS="${BUILD_DIR}/defs"
PSP_TREE="${BUILD_DIR}/psp-tree"
WORK_DIR="${BUILD_DIR}/work"
EXE_DIR="${BUILD_DIR}/exe/cpu1"

LOG_FILE="${LOG_DIR}/10-memory-report${SUFFIX}.log"
REPORT="${LOG_DIR}/10-memory-report${SUFFIX}.txt"
OUT_DIR="${LOG_DIR}/10-memory${SUFFIX}"
MAP_COPY="${OUT_DIR}/core-cpu1.map"

check_tools() {
    local t
    for t in "${QEMU}" "${GDB}" awk sort; do
        command -v "${t}" >/dev/null 2>&1 || die "'${t}' not found — run scripts/01-check-prerequisites.sh"
    done
    "${GDB}" -nx -batch -ex 'python print("py-ok")' 2>/dev/null | grep -q py-ok || die "${GDB} has no Python support"
    [[ -f "${PROBE}" ]] || die "missing ${PROBE}"
    log "OK   $("${QEMU}" --version | head -1); $("${GDB}" --version | head -1)"
}

cleanup() {
    if [[ -n "${QEMU_PID}" ]] && kill -0 "${QEMU_PID}" 2>/dev/null; then
        kill "${QEMU_PID}" 2>/dev/null || true
        wait "${QEMU_PID}" 2>/dev/null || true
    fi
}

build_debug_image() {
    # CMake takes CFLAGS from the environment when it first configures (also in cFE's per-target sub-build).
    export CFLAGS="-g"
    cfe_sim_build "${DO_CLEAN}"
    unset CFLAGS
    "${GDB}" -nx -batch -ex 'ptype TCB_t' -ex 'ptype CFE_SB_Global' "${EXE_DIR}/core-cpu1" >/dev/null 2>&1 \
        || die "the -g image has no type information (CFLAGS=-g not applied? try --clean)"
    log "OK   -g image with types: ${EXE_DIR}/core-cpu1"
    local mine flight
    mine="$("${SIZE}" "${EXE_DIR}/core-cpu1" | awk 'NR==2 {print $2, $3}')"
    if [[ -x "${FLIGHT_EXE}" ]]; then
        flight="$("${SIZE}" "${FLIGHT_EXE}" | awk 'NR==2 {print $2, $3}')"
        [[ "${mine}" == "${flight}" ]] || die "data/bss differ from script ${REF_SCRIPT}'s image (${mine} vs ${flight})"
        log "OK   data and bss equal to script ${REF_SCRIPT}'s image (${mine})"
    else
        log "NOTE script ${REF_SCRIPT}'s image not present: data/bss comparison skipped"
    fi
}

hex_awk='
function hex(x,   i, c, v) {
    sub(/^0[xX]/, "", x); v = 0
    for (i = 1; i <= length(x); i++) { c = index("0123456789abcdef", tolower(substr(x, i, 1))); v = v * 16 + c - 1 }
    return v
}'

# Owner of a link-map input object (archive member or object path).
classify_awk='
function owner(o,   m) {
    if (o ~ /libc_nano\.a|libc\.a|libgcc\.a|libm\.a/) return "newlib/libgcc";
    if (match(o, /lib(es|evs|fs|sb|sbr|tbl|time|config|msg|resourceid|core_api|core_private)\.a/)) {
        m = substr(o, RSTART + 3, RLENGTH - 5); if (m == "sbr") m = "sb"; return "cFE " toupper(m);
    }
    if (o ~ /counter_app/) return "counter_app";
    if (o ~ /\(os-impl-/) return "OSAL FreeRTOS layer (osal-ext)";
    if (o ~ /libosal_bsp\.a/) return "OSAL BSP (osal-ext)";
    if (o ~ /libosal\.a/) return "OSAL shared";
    if (o ~ /libpsp-|libiodriver|libsoft_timebase|libtimebase_freertos|cfe_psp_/) return "PSP";
    if (o ~ /alloc_traps|syscalls|startup|board\.c|diag/) return "board (boards/)";
    if (o ~ /tasks\.c|timers\.c|queue\.c|list\.c|port\.c|event_groups|stream_buffer|bsp_freertos_hooks/) return "FreeRTOS kernel + hooks";
    return "other: " o;
}'

ram_by_owner() {
    local map
    map="$(find "${WORK_DIR}/${ARCH_SUBDIR}" -name core-cpu1.map | head -1)"
    [[ -s "${map}" ]] || die "link map core-cpu1.map not found"
    cp "${map}" "${MAP_COPY}"
    # readelf -SW: "[ N] name type addr off size ..." (the index may split into two fields).
    RAM_LO="$("${READELF}" -SW "${EXE_DIR}/core-cpu1" | awk '{for (i = 1; i < NF; i++) if ($i == ".data") print "0x" $(i + 2)}')"
    [[ -n "${RAM_LO}" ]] || die "no .data section in the image"
    local msp_size
    msp_size="$("${READELF}" -SW "${EXE_DIR}/core-cpu1" | awk "${hex_awk}"'{for (i = 1; i < NF; i++) if ($i == ".main_stack") print hex($(i + 4))}')"
    {
        echo "## Static RAM by owner (link map; .data + .bss input sections in RAM, plus the main stack)"
        awk '/^Linker script and memory map/ {m=1} m' "${MAP_COPY}" | awk -v lo="${RAM_LO}" -v msp="${msp_size:-0}" "${hex_awk}${classify_awk}"'
            /^ (\.bss|\.data|COMMON)/ {
                if (NF >= 4) { a = $2; s = $3; o = $4 } else { getline; a = $1; s = $2; o = $3 }
                if (hex(a) >= hex(lo) && hex(s) > 0) { b[owner(o)] += hex(s); t += hex(s) }
            }
            END {
                b["main stack (.main_stack, MSP)"] += msp; t += msp
                for (k in b) printf "%10d  %5.1f %%  %s\n", b[k], 100.0 * b[k] / t, k | "sort -rn"
                close("sort -rn")
                printf "%10d  100.0 %%  total\n", t
            }'
        echo
        echo "## Largest RAM symbols (nm, >= 4096 bytes)"
        "${NM}" -S --size-sort "${EXE_DIR}/core-cpu1" | awk "${hex_awk}"'$3 ~ /^[bBdD]$/ && hex("0x" $2) >= 4096 {
            printf "%10d  %s\n", hex("0x" $2), $4 }' | sort -rn
        echo
    } >>"${REPORT}"
    # RAM symbols for the probe: "addr size name".
    "${NM}" -S "${EXE_DIR}/core-cpu1" | awk 'NF == 4 && $3 ~ /^[bBdD]$/ {print $1, $2, $4}' >"${OUT_DIR}/symtab.txt"
    log "OK   static RAM table written ($(wc -l <"${OUT_DIR}/symtab.txt") RAM symbols)"
}

run_and_probe() {
    local console="${OUT_DIR}/console.log" probe_out="${OUT_DIR}/probe.txt"
    local waited=0 what
    # Same QEMU options as script 09 / 09c plus the gdb stub on localhost.
    # shellcheck disable=SC2086 # QEMU_OPTS holds several options
    (cd "${EXE_DIR}" && exec "${QEMU}" -M mps2-an385 ${QEMU_OPTS} -nographic \
        -monitor none -serial stdio -gdb "tcp:127.0.0.1:${GDB_PORT}" -kernel core-cpu1 </dev/null >"${console}" 2>&1) &
    QEMU_PID=$!
    if (( FT )); then
        what="the cFE functional tests and the log dump"
        log "QEMU started (pid ${QEMU_PID}), waiting for ${what} (at most ${FT_TIMEOUT} s)"
        until grep -qE '^FT_LOGDUMP: (END|cannot)' "${console}" 2>/dev/null; do
            kill -0 "${QEMU_PID}" 2>/dev/null || die "QEMU exited early — see ${console}"
            (( waited < FT_TIMEOUT )) || die "tests not finished within ${FT_TIMEOUT} s — see ${console}"
            sleep 5
            waited=$(( waited + 5 ))
        done
        grep -q '^FT_LOGDUMP: END' "${console}" || die "log dump failed — see ${console}"
        sleep 2   # let the cFE tasks go back to their idle waits
    else
        what="counter_app for ${RUN_SECONDS} s"
        log "QEMU started (pid ${QEMU_PID}), running ${what}"
        sleep "${RUN_SECONDS}"
    fi
    kill -0 "${QEMU_PID}" 2>/dev/null || die "QEMU exited early — see ${console}"
    M2S_SYMTAB="${OUT_DIR}/symtab.txt" timeout 120 "${GDB}" -nx -batch \
        -ex "target remote 127.0.0.1:${GDB_PORT}" -ex "source ${PROBE}" -ex "detach" \
        "${EXE_DIR}/core-cpu1" >"${probe_out}" 2>&1 || die "gdb probe failed — see ${probe_out}"
    cleanup
    QEMU_PID=""
    grep -q '^MEM END' "${probe_out}" || die "gdb probe did not finish — see ${probe_out}"
    if grep -q '^MEM ERROR' "${probe_out}"; then
        grep '^MEM ERROR' "${probe_out}" | sed 's/^/ERROR /'
        die "gdb probe reported errors — see ${probe_out}"
    fi
    grep -qE 'M2S (TRAP|FAULT|ASSERT|PANIC|STOP)' "${console}" && die "trap/fault line in ${console}"
    if (( ! FT )); then
        grep -q 'counter_app initialized' "${console}" || die "counter_app did not start — see ${console}"
    fi
    log "OK   ran ${what}, no trap line; probe output ${probe_out}"
    {
        echo "## Measured after ${what} under QEMU (gdb stub)"
        echo
        echo "### Task stacks (used = high-water mark; FreeRTOS 0xA5 fill)"
        printf '%-58s %8s %8s %8s %6s\n' "stack:owner" "size" "used" "free" "free%"
        awk '$2=="STACK" { pct = ($5 > 0) ? 100.0 * $7 / $5 : 0; printf "%-58s %8d %8d %8d %5.0f%%\n", $3, $5, $6, $7, pct }' "${probe_out}"
        awk '$2=="MSP" { printf "%-58s %8d %8d %8d %5.0f%%  (zero-filled RAM: approximate)\n", "main stack (MSP)", $3, $4, $3-$4, 100.0*($3-$4)/$3 }' "${probe_out}"
        echo
        echo "### cFE ES memory pools (peak = high-water TailPosition)"
        awk '$2=="POOL" { printf "%-28s total %8d  peak %8d (%4.1f%%)  allocations %d\n", $3, $4, $5, 100.0*$5/$4, $6 }' "${probe_out}"
        echo
        echo "### cFE SB / other cFE"
        awk '$2=="SB" || $2=="CFE" { printf "%-6s %-48s %s\n", $2, $3, $4 }' "${probe_out}"
        echo
        echo "### OSAL objects in use at the end of the run / table limit"
        awk '$2=="OSAL" { printf "%-10s %4d / %4d\n", $3, $4, $5 }' "${probe_out}"
        echo
        echo "### OSAL queues existing at the end of the run (index depth message-size)"
        awk '$2=="QUEUE" { printf "queue %-3s depth %4d  size %6d\n", $3, $4, $5 }' "${probe_out}"
        echo
        echo "### PSP reserved memory"
        awk '$2=="PSP" { printf "%-22s %10d\n", $3, $4 }' "${probe_out}"
    } >>"${REPORT}"
}

main() {
    mkdir -p "${LOG_DIR}" "${OUT_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    trap cleanup EXIT
    log "=== 10-memory-report start (mission ${MISSION}, RUN_SECONDS=${RUN_SECONDS}, FT_TIMEOUT=${FT_TIMEOUT}, GDB_PORT=${GDB_PORT}) ==="
    cfe_sim_check_prerequisites
    check_cross_tools
    check_tools
    local before after
    before="$(cfe_sim_sources_status)"

    build_debug_image
    {
        echo "# Memory report — Cortex-M3 image, mission ${MISSION} (base freertos-arm-cm3, board qemu-mps2-an385)"
        echo "Generated $(date '+%Y-%m-%d %H:%M:%S') by scripts/10-memory-report.sh; image built with -g in ${BUILD_DIR#"${ROOT_DIR}/"}"
        echo "Sections: $("${SIZE}" "${EXE_DIR}/core-cpu1" | awk 'NR==2 {print "text=" $1 " data=" $2 " bss=" $3}')"
        echo
    } >"${REPORT}"
    ram_by_owner
    run_and_probe

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "report: ${REPORT}"
    log "=== 10-memory-report OK ==="
}

main
