#!/usr/bin/env bash
# 03-build-freertos-posix-smoke.sh — build and run the FreeRTOS V11.3.2 POSIX-port smoke test.
#
# No cFE and no OSAL: this only proves the pinned FreeRTOS kernel runs on the Linux host with
#   - project FreeRTOSConfig.h (config/freertos/posix/), static allocation only,
#   - patch 0001 (tick catch-up) applied to a COPY of port.c in the build directory,
#   - the sleeping idle hook (config/freertos/posix/freertos_posix_hooks.c).
#
# Checks:
#   1. build with -Wall -Wextra (test code also -Werror);
#   2. RUNS runs of the smoke binary, all must print "RESULT: PASS" and exit 0
#      (5 x 1 s periods: each within +/-150 ms, total within +/-200 ms; see docs/DECISIONS.md D16);
#   3. no FreeRTOS heap in the link map / symbol table (no heap_*.c, no pvPortMalloc);
#   4. negative test: a build that references pvPortMalloc must fail to link;
#   5. sources/FreeRTOS-Kernel working tree unchanged.
#
# Note: the POSIX port itself uses glibc malloc and pthread stacks. That is host-side only and
# says nothing about the embedded target (D4).
#
# Usage: ./scripts/03-build-freertos-posix-smoke.sh [--clean]      (env: RUNS, default 10)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
TEST_DIR="${ROOT_DIR}/tests/freertos-posix-smoke"
PATCH_FILE="${ROOT_DIR}/patches/freertos-kernel/0001-posix-port-tick-catch-up.patch"
BUILD_DIR="${ROOT_DIR}/build/freertos-posix-smoke"
NEG_BUILD_DIR="${ROOT_DIR}/build/freertos-posix-smoke-negheap"
BIN="${BUILD_DIR}/freertos_posix_smoke"
MAP="${BUILD_DIR}/freertos_posix_smoke.map"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/03-build-freertos-posix-smoke.log"
RUN_LOG="${LOG_DIR}/03-freertos-posix-smoke-run.log"
RUNS="${RUNS:-10}"
RUN_TIMEOUT=30   # one run takes ~5 s; SIGKILL after this (a hung port may ignore SIGTERM)

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
}

check_prerequisites() {
    local tool
    for tool in cmake make gcc patch nm timeout git; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found — run scripts/01-check-prerequisites.sh"
    done
    [[ -f "${KERNEL_DIR}/tasks.c" ]] || die "sources/FreeRTOS-Kernel not populated — run scripts/00-basic-setup.sh"
    [[ -f "${PATCH_FILE}" ]] || die "patch not found: ${PATCH_FILE}"
    [[ -f "${TEST_DIR}/CMakeLists.txt" ]] || die "test not found: ${TEST_DIR}"
}

kernel_tree_status() {
    git -C "${KERNEL_DIR}" status --porcelain --ignore-submodules=none
}

check_patch_applies() {
    patch --dry-run --quiet --forward -o /dev/null \
        "${KERNEL_DIR}/portable/ThirdParty/GCC/Posix/port.c" "${PATCH_FILE}" \
        || die "patch 0001 does not apply cleanly to the pinned port.c"
    log "OK   patch 0001 applies cleanly to the pinned port.c"
}

build() {
    if (( DO_CLEAN )); then
        log "removing ${BUILD_DIR} and ${NEG_BUILD_DIR}"
        rm -rf "${BUILD_DIR}" "${NEG_BUILD_DIR}"
    fi
    log "configuring and building ${BUILD_DIR}"
    cmake -S "${TEST_DIR}" -B "${BUILD_DIR}" -DSMOKE_FORCE_HEAP_USE=OFF
    cmake --build "${BUILD_DIR}" -j"$(nproc)"
    [[ -x "${BIN}" ]] || die "binary not produced: ${BIN}"
    [[ -f "${BUILD_DIR}/freertos_kernel-patched/port.c" ]] || die "patched port.c not generated"
    grep -q "m2s-cfs" "${BUILD_DIR}/freertos_kernel-patched/port.c" || die "generated port.c does not contain the patch"
    log "OK   built with patched port.c"
}

check_no_heap() {
    [[ -f "${MAP}" ]] || die "link map not produced: ${MAP}"
    if grep -Eq 'heap_[1-5]\.c|heap_[1-5]\.o|pvPortMalloc|vPortFree' "${MAP}"; then
        die "FreeRTOS heap found in link map ${MAP}"
    fi
    if nm "${BIN}" | grep -Eq '\b(pvPortMalloc|vPortFree|xPortGetFreeHeapSize)\b'; then
        die "FreeRTOS heap symbol found in ${BIN}"
    fi
    log "OK   no FreeRTOS heap (heap_*.c, pvPortMalloc, vPortFree) in link map or symbols"
}

negative_heap_test() {
    local neg_log="${LOG_DIR}/03-negative-heap-build.log" rc=0
    log "negative test: build referencing pvPortMalloc (must fail to link)"
    rm -rf "${NEG_BUILD_DIR}"
    cmake -S "${TEST_DIR}" -B "${NEG_BUILD_DIR}" -DSMOKE_FORCE_HEAP_USE=ON >"${neg_log}" 2>&1 \
        || die "negative test: configure failed (see ${neg_log})"
    cmake --build "${NEG_BUILD_DIR}" >>"${neg_log}" 2>&1 || rc=$?
    (( rc != 0 )) || die "negative test: build SUCCEEDED but must fail (see ${neg_log})"
    grep -q "undefined reference to .pvPortMalloc" "${neg_log}" \
        || die "negative test: build failed for another reason (see ${neg_log})"
    log "OK   negative test: link failed with undefined reference to pvPortMalloc"
}

smoke_runs() {
    local i rc passed=0
    : >"${RUN_LOG}"
    for (( i = 1; i <= RUNS; i++ )); do
        rc=0
        printf '===== run %d =====\n' "${i}" >>"${RUN_LOG}"
        timeout -s KILL "${RUN_TIMEOUT}" "${BIN}" >>"${RUN_LOG}" 2>&1 || rc=$?
        printf '===== run %d exit %d =====\n' "${i}" "${rc}" >>"${RUN_LOG}"
        local summary
        summary="$(sed -n "/^===== run ${i} =====\$/,/^===== run ${i} exit/p" "${RUN_LOG}" | grep -E '^(total|RESULT):' | tr '\n' ' ')"
        if (( rc == 0 )) && [[ "${summary}" == *"RESULT: PASS"* ]]; then
            passed=$(( passed + 1 ))
            log "OK   run ${i}/${RUNS}: ${summary}"
        else
            log "ERROR run ${i}/${RUNS}: exit ${rc} ${summary:-(no result printed)}"
        fi
    done
    log "smoke runs: ${passed}/${RUNS} passed (output: ${RUN_LOG})"
    (( passed == RUNS )) || die "smoke test failed"
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 03-build-freertos-posix-smoke start (RUNS=${RUNS}) ==="

    check_prerequisites
    local before after
    before="$(kernel_tree_status)"

    check_patch_applies
    build
    check_no_heap
    negative_heap_test
    smoke_runs

    after="$(kernel_tree_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/FreeRTOS-Kernel working tree is not clean"
    log "OK   sources/FreeRTOS-Kernel working tree unchanged and clean"
    log "=== 03-build-freertos-posix-smoke OK ==="
}

main "$@"
