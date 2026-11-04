#!/usr/bin/env bash
# 05-build-cfe-freertos-sim.sh — build stock cFE (core-cpu1, no apps) on the m2s-cfs FreeRTOS OSAL
# layer with the project PSP "freertos-posix-sim", and run it on the Linux simulator (step 05).
#
#   cFE / OSAL : sources/cFS (pinned, unmodified)
#   OS layer   : osal-ext/freertos, BSP osal-ext/freertos-posix-sim (OSAL_EXT_SOURCE_DIR)
#   PSP        : psp-ext/ (platform freertos-posix-sim, module timebase_freertos), added to a build-time
#                copy of sources/cFS/psp, because cFE only finds a PSP platform inside the PSP tree (D27)
#   mission    : config/cfe/freertos-posix-sim (MISSION_DEFS; startup script embedded, D5)
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
ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
CFS_DIR="${ROOT_DIR}/sources/cFS"
KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
PSP_EXT_DIR="${ROOT_DIR}/psp-ext"
MISSION_DEFS="${ROOT_DIR}/config/cfe/freertos-posix-sim"
BUILD_DIR="${ROOT_DIR}/build/cfe-freertos-sim"
PSP_TREE="${BUILD_DIR}/psp-tree"
WORK_DIR="${BUILD_DIR}/work"
EXE_DIR="${BUILD_DIR}/exe/cpu1"
LOG_DIR="${ROOT_DIR}/logs"
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

# Objects allowed to reference malloc/free: the FreeRTOS POSIX port, host-only.
MALLOC_ALLOWED='/(port\.c|wait_for_event\.c)\.o$'

DO_CLEAN=0

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"; }
die() { log "FATAL: $*"; exit 1; }

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

check_prerequisites() {
    local tool
    for tool in cmake make gcc patch nm timeout git; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found — run scripts/01-check-prerequisites.sh"
    done
    [[ -f "${CFS_DIR}/cfe/CMakeLists.txt" && -f "${CFS_DIR}/psp/CMakeLists.txt" ]] \
        || die "sources/cFS not populated — run scripts/00-basic-setup.sh"
    [[ -f "${KERNEL_DIR}/tasks.c" ]] || die "sources/FreeRTOS-Kernel not populated — run scripts/00-basic-setup.sh"
    [[ -d "${PSP_EXT_DIR}/fsw/freertos-posix-sim" && -d "${PSP_EXT_DIR}/fsw/modules/timebase_freertos" ]] \
        || die "psp-ext/ not found"
    [[ -f "${MISSION_DEFS}/targets.cmake" ]] || die "mission config not found: ${MISSION_DEFS}"
}

sources_status() {
    git -C "${CFS_DIR}" status --porcelain --ignore-submodules=none
    git -C "${KERNEL_DIR}" status --porcelain --ignore-submodules=none
}

# Fresh copy of the stock PSP tree plus the project platform and module (D27). Rebuilt every run so it
# always matches sources/cFS/psp and psp-ext/; cp -p keeps timestamps, so unchanged files do not rebuild.
assemble_psp_tree() {
    rm -rf "${PSP_TREE}"
    mkdir -p "${PSP_TREE}"
    cp -a "${CFS_DIR}/psp" "${PSP_TREE}/psp"
    [[ ! -e "${PSP_TREE}/psp/fsw/freertos-posix-sim" ]] || die "stock PSP already has a freertos-posix-sim platform"
    [[ ! -e "${PSP_TREE}/psp/fsw/modules/timebase_freertos" ]] || die "stock PSP already has a timebase_freertos module"
    cp -a "${PSP_EXT_DIR}/fsw/freertos-posix-sim" "${PSP_TREE}/psp/fsw/"
    cp -a "${PSP_EXT_DIR}/fsw/modules/timebase_freertos" "${PSP_TREE}/psp/fsw/modules/"
    log "OK   PSP tree assembled at ${PSP_TREE}/psp (stock PSP + psp-ext)"
}

build() {
    if (( DO_CLEAN )); then
        log "removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
    assemble_psp_tree
    if [[ ! -f "${WORK_DIR}/CMakeCache.txt" ]]; then
        log "configuring ${WORK_DIR}"
        cmake -S "${CFS_DIR}/cfe" -B "${WORK_DIR}" \
            -DMISSION_SOURCE_DIR="${CFS_DIR}" \
            -DMISSIONCONFIG=m2s \
            -DMISSION_DEFS="${MISSION_DEFS}" \
            -DCFS_APP_PATH="${PSP_TREE}" \
            -DCMAKE_INSTALL_PREFIX=/exe
    fi
    log "building mission-all"
    make -C "${WORK_DIR}" -j"$(nproc)" mission-all
    # With no apps there are no tables, but cFE's install rule still copies tables/staging.
    mkdir -p "${WORK_DIR}/tables/staging"
    make -C "${WORK_DIR}" DESTDIR="${BUILD_DIR}" mission-install
    [[ -x "${EXE_DIR}/core-cpu1" ]] || die "core-cpu1 not produced at ${EXE_DIR}"
    log "OK   build and install: ${EXE_DIR}/core-cpu1"
}

check_static_only() {
    local bin="${EXE_DIR}/core-cpu1" obj bad=0 users=0
    if nm "${bin}" | grep -Eq '\b(pvPortMalloc|vPortFree|xTaskCreate|xQueueGenericCreate|xQueueCreateMutex|xQueueCreateCountingSemaphore|xTimerCreate|xEventGroupCreate)$'; then
        die "core-cpu1: FreeRTOS heap or dynamic FreeRTOS object creation symbol found"
    fi
    log "OK   no FreeRTOS heap or dynamic object creation in core-cpu1"

    # Every target object (unit-test stubs and ut_assert are not linked into core-cpu1).
    while IFS= read -r obj; do
        if nm -u "${obj}" 2>/dev/null | grep -Eq '\b(malloc|free|calloc|realloc|posix_memalign|aligned_alloc)$'; then
            users=$(( users + 1 ))
            if [[ ! "${obj}" =~ ${MALLOC_ALLOWED} ]]; then
                log "ERROR malloc/free referenced by ${obj#"${WORK_DIR}/"}"
                bad=1
            fi
        fi
    done < <(find "${WORK_DIR}/freertos-posix-sim" -name '*.o' -path '*CMakeFiles*' \
                 ! -path '*ut-stubs*' ! -path '*ut_assert*' | sort)
    (( bad == 0 )) || die "dynamic allocation outside the host POSIX port"
    log "OK   malloc/free referenced only by the host POSIX port (${users} object(s): port.c, wait_for_event.c)"
    log "     core-cpu1 sections: $(size "${bin}" | tail -1 | awk '{print "text=" $1 " data=" $2 " bss=" $3}')"
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

    check_prerequisites
    local before after
    before="$(sources_status)"

    build
    check_static_only
    run_cfe

    after="$(sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 05-build-cfe-freertos-sim OK ==="
}

main "$@"
