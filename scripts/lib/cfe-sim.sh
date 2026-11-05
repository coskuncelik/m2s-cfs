#!/usr/bin/env bash
# scripts/lib/cfe-sim.sh — shared steps for scripts 05, 05b and 06: build cFE for the FreeRTOS Linux simulator
# and check its static memory. Sourced, not run. The caller sets `set -euo pipefail` and calls
# cfe_sim_init <mission> first.
#
#   cFE / OSAL : sources/cFS (pinned, unmodified)
#   OS layer   : osal-ext/freertos, BSP osal-ext/freertos-posix-sim (OSAL_EXT_SOURCE_DIR)
#   PSP        : psp-ext/, added to a build-time copy of sources/cFS/psp (D27)
#   apps       : apps/ (linked statically)
#   mission    : config/cfe/freertos-posix-sim (base), optionally overlaid by config/cfe/<mission>; assembled
#                in the build directory with m2s_paths.cmake and the embedded /cf image cf.tar (D29)
#
# Missions: freertos-posix-sim (flight: counter_app), freertos-posix-sim-ft (cFE functional tests).

# $1 = mission name (directory under config/cfe/).
cfe_sim_init() {
    MISSION="$1"
    ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
    CFS_DIR="${ROOT_DIR}/sources/cFS"
    KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
    PSP_EXT_DIR="${ROOT_DIR}/psp-ext"
    APPS_DIR="${ROOT_DIR}/apps"
    BASE_DEFS="${ROOT_DIR}/config/cfe/freertos-posix-sim"
    OVERLAY_DEFS="${ROOT_DIR}/config/cfe/${MISSION}"
    [[ -d "${OVERLAY_DEFS}" ]] || die "mission config not found: ${OVERLAY_DEFS}"
    if [[ "${MISSION}" == "freertos-posix-sim" ]]; then
        BUILD_DIR="${ROOT_DIR}/build/cfe-freertos-sim"
    else
        BUILD_DIR="${ROOT_DIR}/build/cfe-${MISSION}"
    fi
    MISSION_DEFS="${BUILD_DIR}/defs"
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
    [[ -f "${BASE_DEFS}/targets.cmake" ]] || die "mission config not found: ${BASE_DEFS}"
    command -v tar >/dev/null 2>&1 || die "'tar' not found"
    command -v xxd >/dev/null 2>&1 || die "'xxd' not found (cFE converts embedded files with it)"
}

# Copies $1 to $2 only if the content differs (keeps timestamps of unchanged files, so nothing rebuilds).
cfe_sim_update_file() {
    if ! cmp -s "$1" "$2" 2>/dev/null; then
        cp -p "$1" "$2"
    fi
}

# Assembles MISSION_DEFS: base config + mission overlay + m2s_paths.cmake + cf.tar. Files in the /cf image:
# cfe_es_startup.scr (from cpu1_cfe_es_startup.scr) and each "<name> <source>" line of cf_files.txt, where
# <source> is relative to EXE_DIR (a previous build pass). Sets CF_MISSING to the number of absent sources.
cfe_sim_assemble_defs() {
    local stage="${BUILD_DIR}/defs.new" cfdir="${BUILD_DIR}/cf-image" name src f
    rm -rf "${stage}" "${cfdir}"
    mkdir -p "${stage}" "${cfdir}" "${MISSION_DEFS}"
    cp -a "${BASE_DEFS}/." "${stage}/"
    if [[ "${OVERLAY_DEFS}" != "${BASE_DEFS}" ]]; then
        cp -a "${OVERLAY_DEFS}/." "${stage}/"
    fi
    printf 'set(M2S_ROOT_DIR "%s")\n' "${ROOT_DIR}" >"${stage}/m2s_paths.cmake"

    cp "${stage}/cpu1_cfe_es_startup.scr" "${cfdir}/cfe_es_startup.scr"
    CF_MISSING=0
    while read -r name src; do
        [[ -z "${name}" || "${name}" == \#* ]] && continue
        if [[ -f "${EXE_DIR}/${src}" ]]; then
            cp "${EXE_DIR}/${src}" "${cfdir}/${name}"
        else
            CF_MISSING=$(( CF_MISSING + 1 ))
        fi
    done <"${stage}/cf_files.txt"
    # Deterministic archive: fixed order, owner and time, so an unchanged /cf does not relink core-cpu1.
    tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 \
        -C "${cfdir}" -cf "${stage}/cf.tar" .

    for f in "${stage}"/*; do
        cfe_sim_update_file "${f}" "${MISSION_DEFS}/${f##*/}"
    done
    for f in "${MISSION_DEFS}"/*; do
        [[ -e "${stage}/${f##*/}" ]] || rm -rf "${f}"
    done
    rm -rf "${stage}"
    log "OK   mission config assembled at ${MISSION_DEFS} (/cf image: $(tar -tf "${MISSION_DEFS}/cf.tar" | grep -vc '/$') file(s), ${CF_MISSING} not built yet)"
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

# One configure (if needed) + build + install pass.
cfe_sim_build_pass() {
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
}

# $1 = 1 for a clean build. Builds twice when the /cf image needs files that only the build produces
# (table images from elf2cfetbl): the first pass makes them, the second embeds them.
cfe_sim_build() {
    local do_clean="$1"
    if (( do_clean )); then
        log "removing ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
    cfe_sim_assemble_psp_tree
    cfe_sim_assemble_defs
    cfe_sim_build_pass
    if (( CF_MISSING > 0 )); then
        log "second build pass to embed the ${CF_MISSING} generated /cf file(s)"
        cfe_sim_assemble_defs
        (( CF_MISSING == 0 )) || die "${CF_MISSING} /cf file(s) listed in cf_files.txt were not built"
        cfe_sim_build_pass
    fi
    log "OK   build and install: ${EXE_DIR}/core-cpu1 (mission ${MISSION})"
}

cfe_sim_check_static_only() {
    local bin="${EXE_DIR}/core-cpu1" obj bad=0 users=0 count=0 syms
    # nm output captured first: with pipefail, "nm | grep -q" can fail when grep exits early (SIGPIPE).
    syms="$(nm "${bin}")"
    if grep -Eq '\b(pvPortMalloc|vPortFree|xTaskCreate|xQueueGenericCreate|xQueueCreateMutex|xQueueCreateCountingSemaphore|xTimerCreate|xEventGroupCreate)$' <<<"${syms}"; then
        die "core-cpu1: FreeRTOS heap or dynamic FreeRTOS object creation symbol found"
    fi
    log "OK   no FreeRTOS heap or dynamic object creation in core-cpu1"
    grep -Eq ' T CFE_PSP_Main$' <<<"${syms}" \
        || die "core-cpu1 does not contain CFE_PSP_Main: the PSP start-up was not linked (OS_Application_Startup from elsewhere)"

    # Every target object (unit-test stubs and ut_assert are not linked into core-cpu1).
    while IFS= read -r obj; do
        count=$(( count + 1 ))
        if grep -Eq '\b(malloc|free|calloc|realloc|posix_memalign|aligned_alloc)$' <<<"$(nm -u "${obj}" 2>/dev/null)"; then
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
