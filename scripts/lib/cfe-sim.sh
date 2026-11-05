#!/usr/bin/env bash
# scripts/lib/cfe-sim.sh — shared steps for scripts 05 and 06: build cFE for the FreeRTOS Linux simulator
# and check its static memory. Sourced, not run. The caller sets `set -euo pipefail`, LOG_FILE and calls
# cfe_sim_init first.
#
#   cFE / OSAL : sources/cFS (pinned, unmodified)
#   OS layer   : osal-ext/freertos, BSP osal-ext/freertos-posix-sim (OSAL_EXT_SOURCE_DIR)
#   PSP        : psp-ext/, added to a build-time copy of sources/cFS/psp (D27)
#   apps       : apps/ (counter_app, linked statically)
#   mission    : config/cfe/freertos-posix-sim (MISSION_DEFS; startup script embedded, D5)

cfe_sim_init() {
    ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
    CFS_DIR="${ROOT_DIR}/sources/cFS"
    KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
    PSP_EXT_DIR="${ROOT_DIR}/psp-ext"
    APPS_DIR="${ROOT_DIR}/apps"
    MISSION_DEFS="${ROOT_DIR}/config/cfe/freertos-posix-sim"
    BUILD_DIR="${ROOT_DIR}/build/cfe-freertos-sim"
    PSP_TREE="${BUILD_DIR}/psp-tree"
    WORK_DIR="${BUILD_DIR}/work"
    EXE_DIR="${BUILD_DIR}/exe/cpu1"
    LOG_DIR="${ROOT_DIR}/logs"
    # Objects allowed to reference malloc/free: the FreeRTOS POSIX port, host-only.
    MALLOC_ALLOWED='/(port\.c|wait_for_event\.c)\.o$'
}

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"; }
die() { log "FATAL: $*"; exit 1; }

cfe_sim_check_prerequisites() {
    local tool
    for tool in cmake make gcc patch nm size timeout git; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found — run scripts/01-check-prerequisites.sh"
    done
    [[ -f "${CFS_DIR}/cfe/CMakeLists.txt" && -f "${CFS_DIR}/psp/CMakeLists.txt" ]] \
        || die "sources/cFS not populated — run scripts/00-basic-setup.sh"
    [[ -f "${KERNEL_DIR}/tasks.c" ]] || die "sources/FreeRTOS-Kernel not populated — run scripts/00-basic-setup.sh"
    [[ -d "${PSP_EXT_DIR}/fsw/freertos-posix-sim" && -d "${PSP_EXT_DIR}/fsw/modules/timebase_freertos" ]] \
        || die "psp-ext/ not found"
    [[ -f "${APPS_DIR}/counter_app/CMakeLists.txt" ]] || die "apps/counter_app not found"
    [[ -f "${MISSION_DEFS}/targets.cmake" ]] || die "mission config not found: ${MISSION_DEFS}"
}

cfe_sim_sources_status() {
    git -C "${CFS_DIR}" status --porcelain --ignore-submodules=none
    git -C "${KERNEL_DIR}" status --porcelain --ignore-submodules=none
}

# Fresh copy of the stock PSP tree plus the project platform and module (D27). Rebuilt every run so it
# always matches sources/cFS/psp and psp-ext/; cp -a keeps timestamps, so unchanged files do not rebuild.
cfe_sim_assemble_psp_tree() {
    rm -rf "${PSP_TREE}"
    mkdir -p "${PSP_TREE}"
    cp -a "${CFS_DIR}/psp" "${PSP_TREE}/psp"
    [[ ! -e "${PSP_TREE}/psp/fsw/freertos-posix-sim" ]] || die "stock PSP already has a freertos-posix-sim platform"
    [[ ! -e "${PSP_TREE}/psp/fsw/modules/timebase_freertos" ]] || die "stock PSP already has a timebase_freertos module"
    cp -a "${PSP_EXT_DIR}/fsw/freertos-posix-sim" "${PSP_TREE}/psp/fsw/"
    cp -a "${PSP_EXT_DIR}/fsw/modules/timebase_freertos" "${PSP_TREE}/psp/fsw/modules/"
    log "OK   PSP tree assembled at ${PSP_TREE}/psp (stock PSP + psp-ext)"
}

# $1 = 1 for a clean build.
cfe_sim_build() {
    local do_clean="$1"
    if (( do_clean )); then
        log "removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
    cfe_sim_assemble_psp_tree
    if [[ ! -f "${WORK_DIR}/CMakeCache.txt" ]]; then
        log "configuring ${WORK_DIR}"
        # Module search: the PSP copy first (so it wins over sources/cFS/psp), then the project apps.
        cmake -S "${CFS_DIR}/cfe" -B "${WORK_DIR}" \
            -DMISSION_SOURCE_DIR="${CFS_DIR}" \
            -DMISSIONCONFIG=m2s \
            -DMISSION_DEFS="${MISSION_DEFS}" \
            -DCFS_APP_PATH="${PSP_TREE}:${APPS_DIR}" \
            -DCMAKE_INSTALL_PREFIX=/exe
    fi
    log "building mission-all"
    make -C "${WORK_DIR}" -j"$(nproc)" mission-all
    # cFE's install rule copies tables/staging even when no app has tables.
    mkdir -p "${WORK_DIR}/tables/staging"
    make -C "${WORK_DIR}" DESTDIR="${BUILD_DIR}" mission-install
    [[ -x "${EXE_DIR}/core-cpu1" ]] || die "core-cpu1 not produced at ${EXE_DIR}"
    log "OK   build and install: ${EXE_DIR}/core-cpu1"
}

cfe_sim_check_static_only() {
    local bin="${EXE_DIR}/core-cpu1" obj bad=0 users=0 count=0
    if nm "${bin}" | grep -Eq '\b(pvPortMalloc|vPortFree|xTaskCreate|xQueueGenericCreate|xQueueCreateMutex|xQueueCreateCountingSemaphore|xTimerCreate|xEventGroupCreate)$'; then
        die "core-cpu1: FreeRTOS heap or dynamic FreeRTOS object creation symbol found"
    fi
    log "OK   no FreeRTOS heap or dynamic object creation in core-cpu1"

    # Every target object (unit-test stubs and ut_assert are not linked into core-cpu1).
    while IFS= read -r obj; do
        count=$(( count + 1 ))
        if nm -u "${obj}" 2>/dev/null | grep -Eq '\b(malloc|free|calloc|realloc|posix_memalign|aligned_alloc)$'; then
            users=$(( users + 1 ))
            if [[ ! "${obj}" =~ ${MALLOC_ALLOWED} ]]; then
                log "ERROR malloc/free referenced by ${obj#"${WORK_DIR}/"}"
                bad=1
            fi
        fi
    done < <(find "${WORK_DIR}/freertos-posix-sim" -name '*.o' -path '*CMakeFiles*' \
                 ! -path '*ut-stubs*' ! -path '*ut_assert*' | sort)
    (( count > 0 )) || die "no target objects found under ${WORK_DIR}/freertos-posix-sim"
    (( bad == 0 )) || die "dynamic allocation outside the host POSIX port"
    log "OK   malloc/free referenced only by the host POSIX port (${users} of ${count} objects: port.c, wait_for_event.c)"
    log "     core-cpu1 sections: $(size "${bin}" | tail -1 | awk '{print "text=" $1 " data=" $2 " bss=" $3}')"
}
