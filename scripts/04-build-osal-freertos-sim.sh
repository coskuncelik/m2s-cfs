#!/usr/bin/env bash
# 04-build-osal-freertos-sim.sh — build OSAL with the m2s-cfs FreeRTOS layer on the Linux
# simulator and run OSAL's functional tests (steps 04a, 04b, 04c).
#
#   OS layer : osal-ext/freertos            (OSAL_SYSTEM_OSTYPE=freertos)
#   BSP      : osal-ext/freertos-posix-sim  (OSAL_SYSTEM_BSPTYPE=freertos-posix-sim)
#   config   : config/osal/freertos-posix-sim/osal_config.cmake, config/freertos/posix/
#   sources  : sources/cFS/osal (pinned, unmodified) via OSAL_EXT_SOURCE_DIR
#
# Checks:
#   0. RAM file system core test (tests/ramfs-core, host gcc with ASan/UBSan, no FreeRTOS);
#   1. configure + build the OSAL library and all tests below (project code: -Wall -Wextra -Werror);
#   2. no FreeRTOS heap and no dynamic FreeRTOS object creation in any test binary;
#   3. each test runs ROUNDS times; every run must exit 0 and report no FAIL/TSF/TTF, except the exact
#      assertions listed in KNOWN_FAILS (each one backed by a decision in docs/DECISIONS.md);
#   4. sources/cFS and sources/FreeRTOS-Kernel unchanged.
#
# Usage: ./scripts/04-build-osal-freertos-sim.sh [--clean]      (env: ROUNDS, default 3)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
OSAL_DIR="${ROOT_DIR}/sources/cFS/osal"
KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
EXT_DIR="${ROOT_DIR}/osal-ext"
OSAL_CONFIG="${ROOT_DIR}/config/osal/freertos-posix-sim/osal_config.cmake"
BUILD_DIR="${ROOT_DIR}/build/osal-freertos-sim"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/04-build-osal-freertos-sim.log"
RUN_DIR="${LOG_DIR}/04-osal-tests"
ROUNDS="${ROUNDS:-3}"
RUN_TIMEOUT=90

# Test programs, as paths relative to the build directory; the CMake target is the file name.
#   tests/       OSAL example/functional tests (04a, 04b, 04c)
#   unit-tests/  OSAL API-exhaustive functional tests (04c)
TESTS=(
    tests/osal-core-test tests/queue-test tests/mutex-test
    tests/bin-sem-test tests/bin-sem-timeout-test tests/count-sem-test tests/count-sem-timeout-test
    tests/idmap-api-test tests/sem-speed-test
    tests/timer-test tests/time-base-api-test tests/timer-add-api-test
    tests/file-api-test tests/file-sys-add-fixed-map-api-test
    tests/condvar-test tests/select-test tests/shell-test tests/symbol-api-test tests/network-api-test
    unit-tests/oscore-test/osal_core_UT unit-tests/osfile-test/osal_file_UT
    unit-tests/osfilesys-test/osal_filesys_UT unit-tests/ostimer-test/osal_timer_UT
    unit-tests/osloader-test/osal_loader_UT unit-tests/osnetwork-test/osal_network_UT
)

# Tolerated assertions, as "<test name>|<text that identifies one FAIL/TSF/TTF line>". A run passes
# only if its failed assertions are exactly the entries listed for that test (same count, each one
# matched), so a new failure or a missing expected one is still an error.
#   D22: on the FreeRTOS POSIX port a task runs on a host pthread stack, not on the stack buffer
#        passed to OS_TaskCreate, so this assertion cannot pass on the simulator.
#   D26: features left out by configuration (osal_config.cmake): the network (network-api-test checks
#        socket address functions without allowing "not implemented") and loading modules from
#        files (osal_loader_UT loads /utmod/MODULE*.so; the dynamic loader is off).
KNOWN_FAILS=(
    "osal-core-test|osal-core-test.c:145 - cpuaddr: VarAddress"
    "network-api-test|network-api-test.c:99 - OS_SocketAddrSetPort(&addr, 0xFFFF) (-28)"
    "network-api-test|network-api-test.c:103 - OS_SocketAddrGetPort(&port, &addr) (-28)"
    "network-api-test|network-api-test.c:108 - OS_SocketAddrInit(&addr, OS_SocketDomain_INET) (-28)"
    "network-api-test|network-api-test.c:113 - OS_SocketAddrFromString(&addr, \"invalid\") (-28)"
    "network-api-test|network-api-test.c:119 - OS_SocketAddrToString(addr_string, 1, &addr) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:128 - OS_ModuleLoad(&module_id, module_name, module_file_name, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:141 - OS_ModuleLoad(&module_id2, \"DUPLICATE\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:156 - OS_ModuleLoad(&module_id, \"Good\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:159 - OS_ModuleUnload(module_id) (-16)"
    "osal_loader_UT|ut_osloader_module_test.c:195 - OS_ModuleLoad(&module_id, \"Good\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_module_test.c:232 - OS_ModuleLoad(&module_id, \"Good\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
    "osal_loader_UT|ut_osloader_symtable_test.c:158 - OS_ModuleLoad(&module_id, \"Mod1\", UT_OS_GENERIC_MODULE_NAME2, OS_MODULE_FLAG_LOCAL_SYMBOLS) (-28)"
)

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
    for tool in cmake make gcc patch nm timeout git; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found — run scripts/01-check-prerequisites.sh"
    done
    [[ -f "${OSAL_DIR}/CMakeLists.txt" ]] || die "sources/cFS not populated — run scripts/00-basic-setup.sh"
    [[ -f "${KERNEL_DIR}/tasks.c" ]] || die "sources/FreeRTOS-Kernel not populated — run scripts/00-basic-setup.sh"
    [[ -d "${EXT_DIR}/freertos" && -d "${EXT_DIR}/freertos-posix-sim" ]] || die "osal-ext/ not found"
    [[ -f "${OSAL_CONFIG}" ]] || die "OSAL config not found: ${OSAL_CONFIG}"
}

sources_status() {
    git -C "${ROOT_DIR}/sources/cFS" status --porcelain --ignore-submodules=none
    git -C "${KERNEL_DIR}" status --porcelain --ignore-submodules=none
}

run_ramfs_core_test() {
    local bin="${BUILD_DIR}/ramfs-core-test"
    mkdir -p "${BUILD_DIR}"
    gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
        -I "${EXT_DIR}/freertos/inc" "${EXT_DIR}/freertos/src/os-ramfs.c" \
        "${ROOT_DIR}/tests/ramfs-core/ramfs_core_test.c" -o "${bin}" \
        || die "cannot build the RAM file system core test (gcc with -fsanitize=address,undefined needed)"
    "${bin}" || die "RAM file system core test failed"
    log "OK   RAM file system core test"
}

build() {
    if (( DO_CLEAN )); then
        log "removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
    log "configuring ${BUILD_DIR}"
    cmake -S "${OSAL_DIR}" -B "${BUILD_DIR}" \
        -DOSAL_SYSTEM_BSPTYPE=freertos-posix-sim \
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

check_static_only() {
    local t bin bad=0
    for t in "${TESTS[@]}"; do
        bin="${BUILD_DIR}/${t}"
        [[ -x "${bin}" ]] || die "test binary missing: ${bin}"
        # FreeRTOS heap or dynamic object creation must not be present at all.
        if nm "${bin}" | grep -Eq '\b(pvPortMalloc|vPortFree|xTaskCreate|xQueueGenericCreate|xQueueCreateMutex|xQueueCreateCountingSemaphore|xTimerCreate|xEventGroupCreate)$'; then
            log "ERROR ${t}: dynamic FreeRTOS allocation symbol found"
            bad=1
        fi
    done
    (( bad == 0 )) || die "dynamic FreeRTOS allocation found"
    log "OK   no FreeRTOS heap or dynamic object creation in any test binary"
}

# Prints the number of KNOWN_FAILS entries for test $1 if every FAIL/TSF/TTF line in log $2 matches
# one of them and the counts are equal; prints nothing otherwise.
known_fails_match() {
    local name="$1" out="$2" entry count=0 line ok
    local -a lines=()
    mapfile -t lines < <(grep -E '^\[ *(FAIL|TSF|TTF)\]' "${out}" || true)
    for entry in "${KNOWN_FAILS[@]}"; do
        [[ "${entry%%|*}" == "${name}" ]] && count=$(( count + 1 ))
    done
    (( count > 0 && count == ${#lines[@]} )) || return 0
    for line in "${lines[@]}"; do
        ok=0
        for entry in "${KNOWN_FAILS[@]}"; do
            if [[ "${entry%%|*}" == "${name}" && "${line}" == *"${entry#*|}"* ]]; then ok=1; break; fi
        done
        (( ok )) || return 0
    done
    echo "${count}"
}

run_tests() {
    local r t name rc fails known total_runs=0 failed_runs=0 out summary na
    rm -rf "${RUN_DIR}"
    mkdir -p "${RUN_DIR}"
    for (( r = 1; r <= ROUNDS; r++ )); do
        for t in "${TESTS[@]}"; do
            name="${t##*/}"
            out="${RUN_DIR}/${name}.round${r}.log"
            rc=0
            (cd "${BUILD_DIR}/${t%/*}" && timeout -s KILL "${RUN_TIMEOUT}" "./${name}") >"${out}" 2>&1 || rc=$?
            fails="$(grep -cE '^\[ *(FAIL|TSF|TTF)\]' "${out}" || true)"
            na="$(grep -cE '^\[ *N/A\]' "${out}" || true)"
            summary="$(grep -E '^\[ *END\] [0-9]+ ' "${out}" | grep -v 'No test cases' | tail -1 | sed 's/  */ /g' || true)"
            total_runs=$(( total_runs + 1 ))
            known="$(known_fails_match "${name}" "${out}")"
            if (( rc == 0 && fails == 0 )); then
                log "OK   round ${r} ${name}: ${summary} N/A::${na}"
            elif [[ -n "${known}" && "${rc}" == 1 ]]; then
                log "OK   round ${r} ${name}: ${summary} N/A::${na} (the ${known} failed assertion(s) are the listed known ones)"
            else
                failed_runs=$(( failed_runs + 1 ))
                log "ERROR round ${r} ${name}: exit ${rc}, ${fails} failed assertion(s): ${summary:-(no summary)}"
                grep -E '^\[ *(FAIL|TSF|TTF)\]' "${out}" | head -10 | sed 's/^/        /' || true
            fi
        done
    done
    log "test runs: $(( total_runs - failed_runs ))/${total_runs} passed (logs: ${RUN_DIR})"
    (( failed_runs == 0 )) || die "OSAL functional tests failed"
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 04-build-osal-freertos-sim start (ROUNDS=${ROUNDS}) ==="

    check_prerequisites
    local before after
    before="$(sources_status)"

    build
    run_ramfs_core_test
    check_static_only
    run_tests

    after="$(sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 04-build-osal-freertos-sim OK ==="
}

main "$@"
