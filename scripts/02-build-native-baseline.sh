#!/usr/bin/env bash
# 02-build-native-baseline.sh — build and smoke-test stock cFS on the native POSIX OSAL.
#
# This is the reference baseline: the pinned cFS is built unmodified, with NASA's
# cfe/cmake/sample_defs used read-only. All output goes under build/native-baseline/.
# Nothing in sources/ is written.
#
# Smoke test: run core-cpu1 -R PO (forced power-on reset, so leftover shared memory from an
# earlier run cannot turn it into a processor reset) for RUN_SECONDS and require that
#   - the PSP starts with a POWER ON reset,
#   - cFE ES reaches "entering OPERATIONAL state", and
#   - SAMPLE_APP reports "Sample App Initialized".
#
# Host side effects of running core-cpu1 (pc-linux PSP, by design):
#   - three SysV shared memory segments (CDS, reset area, user reserved) are kept after exit
#     for processor-reset simulation (see `ipcs -m`);
#   - CI_LAB listens on UDP port 1234 while running;
#   - EEPROM.DAT is created in the install directory.
#
# Usage: ./scripts/02-build-native-baseline.sh [--no-run] [--clean]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
CFS_DIR="${ROOT_DIR}/sources/cFS"
BUILD_DIR="${ROOT_DIR}/build/native-baseline"
EXE_DIR="${BUILD_DIR}/exe/cpu1"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/02-build-native-baseline.log"
RUN_LOG="${LOG_DIR}/02-native-baseline-run.log"
RUN_SECONDS="${RUN_SECONDS:-10}"

DO_RUN=1
DO_CLEAN=0

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"; }
die() { log "FATAL: $*"; exit 1; }

parse_args() {
    local arg
    for arg in "$@"; do
        case "${arg}" in
            --no-run) DO_RUN=0 ;;
            --clean)  DO_CLEAN=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--no-run] [--clean])" ;;
        esac
    done
}

check_prerequisites() {
    "${SCRIPT_DIR}/01-check-prerequisites.sh" >/dev/null \
        || die "host prerequisites missing — run scripts/01-check-prerequisites.sh"
    [[ -f "${CFS_DIR}/cfe/CMakeLists.txt" ]] \
        || die "sources/cFS not populated — run scripts/00-basic-setup.sh"
    [[ -d "${CFS_DIR}/cfe/cmake/sample_defs" ]] || die "cfe/cmake/sample_defs not found"
    command -v timeout >/dev/null 2>&1 || die "timeout (coreutils) not found"
}

source_tree_status() {
    git -C "${CFS_DIR}" status --porcelain --ignore-submodules=none
}

configure() {
    if (( DO_CLEAN )) && [[ -d "${BUILD_DIR}" ]]; then
        log "removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
    log "configuring (SIMULATION=native, MISSIONCONFIG=sample)"
    cmake -S "${CFS_DIR}/cfe" -B "${BUILD_DIR}" \
        -DMISSION_SOURCE_DIR="${CFS_DIR}" \
        -DMISSIONCONFIG=sample \
        -DMISSION_DEFS="${CFS_DIR}/cfe/cmake/sample_defs" \
        -DSIMULATION=native \
        -DCMAKE_INSTALL_PREFIX=/exe
}

build_and_install() {
    log "building"
    make -C "${BUILD_DIR}" -j"$(nproc)" mission-all
    log "installing into ${BUILD_DIR}/exe"
    make -C "${BUILD_DIR}" DESTDIR="${BUILD_DIR}" mission-install
    [[ -x "${EXE_DIR}/core-cpu1" ]] || die "core-cpu1 not produced at ${EXE_DIR}"
}

smoke_run() {
    log "running core-cpu1 for ${RUN_SECONDS}s (output: ${RUN_LOG})"
    local rc=0
    (cd "${EXE_DIR}" && timeout -s INT "${RUN_SECONDS}" ./core-cpu1 -R PO) >"${RUN_LOG}" 2>&1 || rc=$?
    # 124 = stopped by timeout (expected). Anything else means it exited or crashed early.
    [[ ${rc} -eq 124 ]] || die "core-cpu1 ended with status ${rc} before the timeout (see ${RUN_LOG})"

    local failed=0 pattern
    for pattern in "Starting the cFE with a POWER ON reset" "CFE_ES_Main entering OPERATIONAL state" "SAMPLE_APP 1: Sample App Initialized"; do
        if grep -qF -- "${pattern}" "${RUN_LOG}"; then
            log "OK   found: ${pattern}"
        else
            log "ERROR missing: ${pattern}"
            failed=1
        fi
    done
    (( failed == 0 )) || die "smoke test failed (see ${RUN_LOG})"
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 02-build-native-baseline start ==="

    check_prerequisites
    local before after
    before="$(source_tree_status)"

    configure
    build_and_install
    if (( DO_RUN )); then smoke_run; else log "smoke run skipped (--no-run)"; fi

    after="$(source_tree_status)"
    [[ "${before}" == "${after}" ]] || die "sources/cFS working tree changed during the build"
    log "OK   sources/cFS working tree unchanged"
    log "=== 02-build-native-baseline OK ==="
}

main "$@"
