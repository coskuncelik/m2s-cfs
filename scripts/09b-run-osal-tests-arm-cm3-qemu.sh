#!/usr/bin/env bash
# 09b-run-osal-tests-arm-cm3-qemu.sh — OSAL functional tests on an emulated Cortex-M3 (QEMU mps2-an385),
# step 09b (D33).
#
# The same 25 test programs as script 04 (scripts/lib/osal-tests.sh), built for the Cortex-M3 with the OSAL
# FreeRTOS layer, the OSAL BSP freertos-arm-cm3 and the board boards/qemu-mps2-an385
# (config/osal/freertos-arm-cm3/toolchain.cmake). These are TEST images: OSAL's test library ut_assert
# allocates with malloc, so they link the static test-only allocator boards/common-cm3/test_heap.c instead
# of the allocator traps (flight images never do). Each program runs as its own QEMU guest and ends QEMU
# with its exit status through semihosting.
#
# Checks:
#   1. configure + build the OSAL library and all tests (cross compiler; project code -Wall -Wextra -Werror);
#   2. every test image: no FreeRTOS heap or dynamic FreeRTOS object creation; test_heap.c linked and the
#      allocator traps not; newlib's allocator objects absent from its link map;
#   3. each test runs ROUNDS times: QEMU exit status 0 and no FAIL/TSF/TTF, or exit status 1 with exactly the
#      configuration-related failures (D26); never an "M2S TRAP/FAULT/ASSERT/PANIC/STOP" line; a run that
#      does not end within RUN_TIMEOUT fails. D22's simulator-only failure (osal-core-test 02.003) is NOT
#      allowed here: on the Cortex-M3 a task runs on the stack OSAL gives it;
#   4. sources/cFS and sources/FreeRTOS-Kernel unchanged.
#
# Usage: ./scripts/09b-run-osal-tests-arm-cm3-qemu.sh [--clean]      (env: ROUNDS, default 3)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
OSAL_DIR="${ROOT_DIR}/sources/cFS/osal"
KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
EXT_DIR="${ROOT_DIR}/osal-ext"
OSAL_CONFIG="${ROOT_DIR}/config/osal/freertos-arm-cm3/osal_config.cmake"
TOOLCHAIN="${ROOT_DIR}/config/osal/freertos-arm-cm3/toolchain.cmake"
BUILD_DIR="${ROOT_DIR}/build/osal-freertos-arm-cm3"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/09b-run-osal-tests-arm-cm3-qemu.log"
RUN_DIR="${LOG_DIR}/09b-osal-tests"
ROUNDS="${ROUNDS:-3}"
RUN_TIMEOUT=180

QEMU="qemu-system-arm"
QEMU_MACHINE="mps2-an385"
# Guest time from the instruction count, held to real time (D32); semihosting lets the guest end QEMU
# with its exit status (D33).
QEMU_ARGS=(-M "${QEMU_MACHINE}" -icount shift=auto,sleep=on,align=on -semihosting-config enable=on,target=native
           -nographic -monitor none -serial stdio)
NM="arm-none-eabi-nm"

# shellcheck source=lib/osal-tests.sh
source "${SCRIPT_DIR}/lib/osal-tests.sh"
TESTS=("${OSAL_TESTS[@]}")
KNOWN_FAILS=("${OSAL_KNOWN_FAILS_CONFIG[@]}" "${OSAL_KNOWN_FAILS_NO_NETWORK[@]}")   # D26 (networking off on the Cortex-M3, D41); no D22 entry

# Lines that fail a run whatever its exit status (boards/common-cm3/m2s_cm3_diag.h).
M2S_ERROR_RE='^M2S (TRAP|FAULT|ASSERT|PANIC|STOP)'

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
    [[ "${ROUNDS}" =~ ^[1-9][0-9]*$ ]] || die "ROUNDS must be a positive integer (got '${ROUNDS}')"
}

check_prerequisites() {
    local tool
    for tool in cmake make git timeout arm-none-eabi-gcc "${NM}" "${QEMU}"; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found — run scripts/01-check-prerequisites.sh"
    done
    "${QEMU}" -machine help | grep -qE "^${QEMU_MACHINE}[[:space:]]" || die "${QEMU} has no machine ${QEMU_MACHINE}"
    [[ -f "${OSAL_DIR}/CMakeLists.txt" ]] || die "sources/cFS not populated — run scripts/00-basic-setup.sh"
    [[ -f "${KERNEL_DIR}/tasks.c" ]] || die "sources/FreeRTOS-Kernel not populated — run scripts/00-basic-setup.sh"
    [[ -f "${OSAL_CONFIG}" && -f "${TOOLCHAIN}" ]] || die "OSAL Cortex-M3 config not found under config/osal/freertos-arm-cm3"
    log "OK   $(arm-none-eabi-gcc --version | head -1); $("${QEMU}" --version | head -1)"
}

sources_status() {
    git -C "${ROOT_DIR}/sources/cFS" status --porcelain --ignore-submodules=none
    git -C "${KERNEL_DIR}" status --porcelain --ignore-submodules=none
}

build() {
    if (( DO_CLEAN )); then
        log "removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
    log "configuring ${BUILD_DIR} (Cortex-M3, board qemu-mps2-an385, test allocator)"
    cmake -S "${OSAL_DIR}" -B "${BUILD_DIR}" -Wno-dev \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
        -DOSAL_SYSTEM_BSPTYPE=freertos-arm-cm3 \
        -DOSAL_SYSTEM_OSTYPE=freertos \
        -DOSAL_EXT_SOURCE_DIR="${EXT_DIR}" \
        -DOSAL_CONFIGURATION_FILE="${OSAL_CONFIG}" \
        -DOSAL_INSTALL_LIBRARIES=OFF \
        -DENABLE_UNIT_TESTS=ON
    log "building osal and ${#TESTS[@]} tests"
    local t targets=()
    for t in "${TESTS[@]}"; do targets+=("${t##*/}"); done
    cmake --build "${BUILD_DIR}" -j"$(nproc)" --target osal "${targets[@]}"
    log "OK   build"
}

check_images() {
    local t bin syms bad=0
    for t in "${TESTS[@]}"; do
        bin="${BUILD_DIR}/${t}"
        [[ -f "${bin}" && -f "${bin}.map" ]] || die "test image or link map missing: ${bin}"
        # nm output captured first: with pipefail, "nm | grep -q" can fail when grep exits early (SIGPIPE).
        syms="$("${NM}" "${bin}")"
        if grep -Eq '\b(pvPortMalloc|vPortFree|xTaskCreate|xQueueGenericCreate|xQueueCreateMutex|xQueueCreateCountingSemaphore|xTimerCreate|xEventGroupCreate)$' <<<"${syms}"; then
            log "ERROR ${t}: FreeRTOS heap or dynamic FreeRTOS object creation symbol found"
            bad=1
        fi
        if ! grep -Eq ' T M2S_TestHeap_Report$' <<<"${syms}" || grep -Eq ' [BbDd] M2S_AllocTrapWhich$' <<<"${syms}"; then
            log "ERROR ${t}: expected the test allocator (test_heap.c) and not the allocator traps"
            bad=1
        fi
        if grep -Eq 'lib_a-(nano-)?(mallocr|freer|callocr|reallocr|malignr|msizer|sbrkr)\.o' "${bin}.map"; then
            log "ERROR ${t}: newlib allocator objects in the link map"
            bad=1
        fi
    done
    (( bad == 0 )) || die "test image checks failed"
    log "OK   ${#TESTS[@]} test images: no FreeRTOS heap or dynamic objects; test allocator only (test_heap.c), no newlib allocator"
}

run_tests() {
    local r t name rc fails known total_runs=0 failed_runs=0 out summary na heap m2s
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"
    for (( r = 1; r <= ROUNDS; r++ )); do
        for t in "${TESTS[@]}"; do
            name="${t##*/}"
            out="${RUN_DIR}/${name}.round${r}.log"
            rc=0
            # stdin /dev/null: QEMU's -serial stdio reads it, and from timeout's background process group a
            # terminal read stops QEMU (SIGTTIN).
            (cd "${BUILD_DIR}/${t%/*}" && timeout -s KILL "${RUN_TIMEOUT}" "${QEMU}" "${QEMU_ARGS[@]}" -kernel "${name}" </dev/null) 2>&1 \
                | tr -d '\r' >"${out}" || rc="${PIPESTATUS[0]}"
            fails="$(grep -cE '^\[ *(FAIL|TSF|TTF)\]' "${out}" || true)"
            na="$(grep -cE '^\[ *N/A\]' "${out}" || true)"
            summary="$(grep -E '^\[ *END\] [0-9]+ ' "${out}" | grep -v 'No test cases' | tail -1 | sed 's/  */ /g' || true)"
            heap="$(sed -nE 's/^M2S TEST HEAP used ([0-9]+) of .*/\1/p' "${out}" | tail -1)"
            m2s="$(grep -E "${M2S_ERROR_RE}" "${out}" | head -1 || true)"
            total_runs=$(( total_runs + 1 ))
            known="$(known_fails_match "${name}" "${out}")"
            if [[ -z "${m2s}" ]] && (( rc == 0 && fails == 0 )); then
                log "OK   round ${r} ${name}: ${summary} N/A::${na} heap ${heap:-?} B"
            elif [[ -z "${m2s}" && -n "${known}" && "${rc}" == 1 ]]; then
                log "OK   round ${r} ${name}: ${summary} N/A::${na} heap ${heap:-?} B (the ${known} failed assertion(s) are the listed known ones)"
            else
                failed_runs=$(( failed_runs + 1 ))
                log "ERROR round ${r} ${name}: exit ${rc}$( (( rc == 137 )) && echo " (timeout ${RUN_TIMEOUT}s)"), ${fails} failed assertion(s): ${summary:-(no summary)}"
                [[ -n "${m2s}" ]] && log "        ${m2s}"
                grep -E '^\[ *(FAIL|TSF|TTF)\]' "${out}" | head -10 | sed 's/^/        /' || true
            fi
        done
    done
    log "test runs: $(( total_runs - failed_runs ))/${total_runs} passed (logs: ${RUN_DIR})"
    (( failed_runs == 0 )) || die "OSAL functional tests on the Cortex-M3 failed"
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 09b-run-osal-tests-arm-cm3-qemu start (ROUNDS=${ROUNDS}) ==="

    check_prerequisites
    local before after
    before="$(sources_status)"

    build
    check_images
    run_tests

    after="$(sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 09b-run-osal-tests-arm-cm3-qemu OK ==="
}

main "$@"
