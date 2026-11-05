#!/usr/bin/env bash
# 07-check-bundle.sh — verify the shareable bundle cfs-m2s-freertos (step 07, D30).
#
# Checks:
#   1. two exports (scripts/07-export-bundle.sh) give the same git tree;
#   2. every upstream file in the bundle is byte-identical to the pinned working tree, except the listed
#      changes; the bundle has no file outside upstream + added + generated;
#   3. no private references (the export checks this; repeated here on the committed files);
#   4. a fresh git clone builds with the stock commands: make prep && make && make install;
#   5. the clone's core-cpu1 passes the counter_app checks of script 06 RUNS times;
#   6. the clone's core-cpu1: no FreeRTOS heap, malloc/free only in the host POSIX port;
#   7. sources/ unchanged.
#
# Usage: ./scripts/07-check-bundle.sh
#        (env: RUNS default 5, RUN_SECONDS default 20, TOLERANCE_MS default 50, DRIFT_MS default 10)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
# shellcheck source=lib/counter-check.sh
source "${SCRIPT_DIR}/lib/counter-check.sh"
cfe_sim_init freertos-posix-sim
LOG_FILE="${LOG_DIR}/07-check-bundle.log"
RUN_DIR="${LOG_DIR}/07-bundle-runs"
RUNS="${RUNS:-5}"
RUN_SECONDS="${RUN_SECONDS:-20}"
TOLERANCE_MS="${TOLERANCE_MS:-50}"
DRIFT_MS="${DRIFT_MS:-10}"

BUNDLE="${ROOT_DIR}/build/export/cfs-m2s-freertos"
CLONE="${ROOT_DIR}/build/export/clone/cfs-m2s-freertos"

# Upstream paths that differ on purpose (cFS root): replaced, renamed or left out.
CHANGED_UPSTREAM='^(\.gitignore|README\.md|\.gitmodules|\.github/.*)$'
# Files the bundle adds besides the port directories (sample_defs is a copy of cfe/cmake/sample_defs).
GENERATED='^(Makefile|README\.md|README-cFS\.md|PORTING\.md|NOTICE|\.gitignore|sample_defs/.*|osal/src/os/freertos/.*|osal/src/bsp/freertos-posix-sim/.*|psp/fsw/freertos-posix-sim/.*|psp/fsw/modules/timebase_freertos/.*|apps/counter_app/.*)$'

check_reproducible() {
    local t1 t2
    "${SCRIPT_DIR}/07-export-bundle.sh" >/dev/null
    t1="$(git -C "${BUNDLE}" rev-parse 'HEAD^{tree}')"
    "${SCRIPT_DIR}/07-export-bundle.sh" >/dev/null
    t2="$(git -C "${BUNDLE}" rev-parse 'HEAD^{tree}')"
    [[ "${t1}" == "${t2}" ]] || die "two exports gave different trees (${t1} / ${t2})"
    log "OK   two exports give the same tree ${t2:0:12}"
}

check_upstream_identical() {
    local f n=0 bad=0 extra
    while IFS= read -r -d '' f; do
        [[ -f "${CFS_DIR}/${f}" || -L "${CFS_DIR}/${f}" ]] || continue
        [[ "${f}" =~ ${CHANGED_UPSTREAM} ]] && continue
        n=$(( n + 1 ))
        cmp -s "${CFS_DIR}/${f}" "${BUNDLE}/${f}" || { log "ERROR differs from upstream: ${f}"; bad=$(( bad + 1 )); }
    done < <(cd "${CFS_DIR}" && git ls-files -z --recurse-submodules)
    while IFS= read -r -d '' f; do
        [[ -f "${KERNEL_DIR}/${f}" || -L "${KERNEL_DIR}/${f}" ]] || continue
        n=$(( n + 1 ))
        cmp -s "${KERNEL_DIR}/${f}" "${BUNDLE}/FreeRTOS-Kernel/${f}" || { log "ERROR differs from upstream: FreeRTOS-Kernel/${f}"; bad=$(( bad + 1 )); }
    done < <(cd "${KERNEL_DIR}" && git ls-files -z)
    cmp -s "${CFS_DIR}/README.md" "${BUNDLE}/README-cFS.md" || { log "ERROR README-cFS.md differs from NASA's README.md"; bad=$(( bad + 1 )); }
    [[ ! -e "${BUNDLE}/.github" && ! -e "${BUNDLE}/.gitmodules" ]] || { log "ERROR .github or .gitmodules present"; bad=$(( bad + 1 )); }
    (( bad == 0 )) || die "${bad} upstream file(s) changed"

    # Every committed file is upstream (cFS or kernel) or one of the added/generated files.
    extra="$(comm -23 <(git -C "${BUNDLE}" ls-files | LC_ALL=C sort) \
                      <( { (cd "${CFS_DIR}" && git ls-files --recurse-submodules); \
                           (cd "${KERNEL_DIR}" && git ls-files | sed 's|^|FreeRTOS-Kernel/|'); } | LC_ALL=C sort) \
             | grep -vE "${GENERATED}" || true)"
    [[ -z "${extra}" ]] || die "unexpected files in the bundle: ${extra}"
    log "OK   ${n} upstream files byte-identical; only the listed upstream changes; no unexpected files"
}

check_no_leaks() {
    local hits
    hits="$(cd "${BUNDLE}" && git grep -nE '\bD[0-9]{1,2}\b|DECISIONS|STATUS\.md|prompt\.md|CLAUDE|[Ss]teps? 0[0-9]|m2s-cfs|scripts/0|osal-ext|psp-ext|/workspaces|codespace' \
            -- osal/src/os/freertos osal/src/bsp/freertos-posix-sim psp/fsw/freertos-posix-sim \
               psp/fsw/modules/timebase_freertos apps/counter_app README.md PORTING.md NOTICE .gitignore Makefile \
               sample_defs/targets.cmake sample_defs/toolchain-freertos-posix-sim.cmake \
               sample_defs/freertos-posix-sim_osconfig.cmake sample_defs/arch_build_custom.cmake \
               sample_defs/mission_build_custom.cmake sample_defs/cpu1_cfe_es_startup.scr || true)"
    [[ -z "${hits}" ]] || { printf '%s\n' "${hits}"; die "private references in the committed bundle"; }
    [[ -z "$(git -C "${BUNDLE}" ls-files | grep -E '^(docs|scripts|logs|tests|\.claude)/|^(CLAUDE|prompt)\.md$' || true)" ]] \
        || die "private files in the bundle"
    log "OK   no private references or private files in the committed bundle"
}

build_clone() {
    rm -rf "${CLONE}"
    mkdir -p "${CLONE%/*}"
    git clone -q "${BUNDLE}" "${CLONE}"
    log "building a fresh clone with the stock commands (make prep && make && make install)"
    (cd "${CLONE}" && make prep && make -j"$(nproc)" && make install) >"${LOG_DIR}/07-bundle-build.log" 2>&1 \
        || { tail -30 "${LOG_DIR}/07-bundle-build.log"; die "bundle build failed (see logs/07-bundle-build.log)"; }
    [[ -x "${CLONE}/build/exe/cpu1/core-cpu1" ]] || die "core-cpu1 not produced in the clone"
    [[ -z "$(git -C "${CLONE}" status --porcelain)" ]] || die "the build left untracked or changed files in the clone: $(git -C "${CLONE}" status --porcelain | head -5)"
    log "OK   clone built: build/exe/cpu1/core-cpu1; clone working tree still clean (cf.tar and build/ ignored)"
}

main() {
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 07-check-bundle start (RUNS=${RUNS}, RUN_SECONDS=${RUN_SECONDS}, TOLERANCE_MS=${TOLERANCE_MS}, DRIFT_MS=${DRIFT_MS}) ==="
    [[ -n "${EPOCHREALTIME:-}" ]] || die "bash 5 or later needed (EPOCHREALTIME)"
    cfe_sim_check_prerequisites
    local before after
    before="$(cfe_sim_sources_status)"

    check_reproducible
    check_upstream_identical
    check_no_leaks
    build_clone

    EXE_DIR="${CLONE}/build/exe/cpu1"
    WORK_DIR="${CLONE}/build"
    cfe_sim_check_static_only
    run_counter_app

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 07-check-bundle OK ==="
}

main "$@"
