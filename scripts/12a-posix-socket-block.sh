#!/usr/bin/env bash
# 12a-posix-socket-block.sh — step 12a phase 1 (D41): measure how a FreeRTOS POSIX-port task behaves while it
# waits in a host socket call (blocking select() versus zero-timeout polling with vTaskDelay).
#
# Builds tests/freertos-posix-socket-block (pinned FreeRTOS kernel, patch 0001, project FreeRTOSConfig.h, static
# allocation only; no cFE, no OSAL) and runs it RUNS times. Each run prints a baseline and four scenario lines
# (see the test's main.c). This script checks only that every run completes ("RESULT: DONE", exit 0) and that
# sources/FreeRTOS-Kernel is unchanged; the measurements are judged in the test report.
#
# Usage: ./scripts/12a-posix-socket-block.sh [--clean]      (env: RUNS, default 3)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
TEST_DIR="${ROOT_DIR}/tests/freertos-posix-socket-block"
BUILD_DIR="${ROOT_DIR}/build/freertos-posix-socket-block"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/12a-posix-socket-block.log"
RUN_LOG="${LOG_DIR}/12a-posix-socket-block-runs.log"
RUNS="${RUNS:-3}"

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"; }
die() { log "FATAL: $*"; exit 1; }

main() {
    local arg do_clean=0 i rc before after
    for arg in "$@"; do
        case "${arg}" in
            --clean) do_clean=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean])" ;;
        esac
    done
    [[ "${RUNS}" =~ ^[1-9][0-9]*$ ]] || die "RUNS must be a positive integer (got '${RUNS}')"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 12a-posix-socket-block start (RUNS=${RUNS}) ==="
    for arg in cmake make gcc patch timeout git; do
        command -v "${arg}" >/dev/null 2>&1 || die "'${arg}' not found — run scripts/01-check-prerequisites.sh"
    done
    [[ -f "${KERNEL_DIR}/tasks.c" ]] || die "sources/FreeRTOS-Kernel not populated — run scripts/00-basic-setup.sh"
    before="$(git -C "${KERNEL_DIR}" status --porcelain)"

    (( do_clean )) && rm -rf "${BUILD_DIR}"
    cmake -S "${TEST_DIR}" -B "${BUILD_DIR}" >/dev/null
    cmake --build "${BUILD_DIR}" -j"$(nproc)" >/dev/null || die "build failed"
    log "OK   built ${BUILD_DIR}/socket_block"

    : >"${RUN_LOG}"
    for (( i = 1; i <= RUNS; i++ )); do
        printf '===== run %d =====\n' "${i}" >>"${RUN_LOG}"
        rc=0
        timeout 60 "${BUILD_DIR}/socket_block" </dev/null >>"${RUN_LOG}" 2>&1 || rc=$?
        printf '===== run %d exit %d =====\n' "${i}" "${rc}" >>"${RUN_LOG}"
        sed -n "/^===== run ${i} =====/,/^===== run ${i} exit/p" "${RUN_LOG}" | grep -E '^(BASELINE|SCENARIO|RESULT)' \
            | sed "s/^/     run ${i}: /"
        (( rc == 0 )) || die "run ${i} exit ${rc} (see ${RUN_LOG})"
        grep -q '^RESULT: DONE' <(sed -n "/^===== run ${i} =====/,/^===== run ${i} exit/p" "${RUN_LOG}") \
            || die "run ${i}: no 'RESULT: DONE'"
    done

    after="$(git -C "${KERNEL_DIR}" status --porcelain)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/FreeRTOS-Kernel working tree changed"
    log "OK   sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 12a-posix-socket-block OK (${RUNS} runs; measurements in ${RUN_LOG}) ==="
}

main "$@"
