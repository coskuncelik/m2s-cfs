#!/usr/bin/env bash
# 07-check-bundle.sh — verify the shareable bundle cfs-m2s-freertos (steps 07 and 07b, D30, D35).
#
# Checks:
#   1. two exports (scripts/07-export-bundle.sh) give the same git tree; the bundle is either unchanged
#      against the published main or one commit directly on it (history kept, no force push needed);
#   2. every upstream file in the bundle is byte-identical to the pinned working tree, except the listed
#      changes; the bundle has no file outside upstream + added + generated;
#   3. no private references (the export checks this; repeated here on the committed files);
#   4. a fresh git clone builds both configurations with the stock commands
#      (make MISSIONCONFIG=<name> O=<dir> prep && make O=<dir> && make O=<dir> install);
#   5. freertos_sim: no FreeRTOS heap, malloc/free only in the host POSIX port; counter_app checks of
#      script 06, RUNS times;
#   6. freertos_qemu: the Cortex-M3 image checks of scripts 08/09 (ELF/ARMv7-M, vectors, no .got outside
#      .data, no allocator or heap linked, no object referencing malloc/free); counter_app under QEMU
#      (-icount), RUNS times, with script 09's checks;
#   7. the builds leave the clone's working tree clean; sources/ unchanged.
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
QEMU_RUN_CMD="qemu-system-arm -M mps2-an385 -icount shift=auto,sleep=on,align=on -nographic -monitor none -serial stdio -kernel core-cpu1"
CLONE="${ROOT_DIR}/build/export/clone/cfs-m2s-freertos"

# Upstream paths that differ on purpose (cFS root): replaced, renamed or left out.
CHANGED_UPSTREAM='^(\.gitignore|README\.md|\.gitmodules|\.github/.*)$'
# Files the bundle adds besides the port directories (each *_defs is a copy of cfe/cmake/sample_defs).
GENERATED='^(Makefile|README\.md|README-cFS\.md|PORTING\.md|NOTICE|\.gitignore|freertos_sim_defs/.*|freertos_qemu_defs/.*|osal/src/os/freertos/.*|osal/src/bsp/freertos-posix-sim/.*|osal/src/bsp/freertos-arm-cm3/.*|psp/fsw/freertos-posix-sim/.*|psp/fsw/freertos-arm-cm3/.*|psp/fsw/modules/timebase_freertos/.*|apps/counter_app/.*|ground_station/.*)$'

check_cross_tools_and_qemu() {
    local tool
    for tool in arm-none-eabi-gcc arm-none-eabi-nm arm-none-eabi-objdump arm-none-eabi-readelf qemu-system-arm; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found — run scripts/01-check-prerequisites.sh"
    done
}

check_reproducible() {
    local t1 t2
    "${SCRIPT_DIR}/07-export-bundle.sh" >/dev/null
    t1="$(git -C "${BUNDLE}" rev-parse 'HEAD^{tree}')"
    "${SCRIPT_DIR}/07-export-bundle.sh" >/dev/null
    t2="$(git -C "${BUNDLE}" rev-parse 'HEAD^{tree}')"
    [[ "${t1}" == "${t2}" ]] || die "two exports gave different trees (${t1} / ${t2})"
    # Either unchanged (HEAD is the published main) or one new commit directly on it (D35a).
    if [[ "$(git -C "${BUNDLE}" rev-parse HEAD)" == "$(git -C "${BUNDLE}" rev-parse origin/main)" ]]; then
        log "OK   two exports give the same tree ${t2:0:12}; bundle unchanged against published main $(git -C "${BUNDLE}" rev-parse --short=12 origin/main)"
    else
        [[ "$(git -C "${BUNDLE}" rev-parse HEAD^)" == "$(git -C "${BUNDLE}" rev-parse origin/main)" ]] \
            || die "the bundle commit is not directly on the published main"
        log "OK   two exports give the same tree ${t2:0:12}; commit $(git -C "${BUNDLE}" rev-parse --short=12 HEAD) on published main $(git -C "${BUNDLE}" rev-parse --short=12 origin/main)"
    fi
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
    local hits re='\bD[0-9]{1,2}\b|DECISIONS|STATUS\.md|prompt\.md|CLAUDE|[Ss]teps? 0[0-9]|m2s-cfs|scripts/0|osal-ext|psp-ext|/workspaces|codespace'
    local -a defs=()
    local c f
    # *_defs: only the files the bundle adds or replaces (the others are cFE's stock sample_defs).
    for c in freertos_sim freertos_qemu; do
        while IFS= read -r f; do
            if [[ ! -e "${BUNDLE}/cfe/cmake/sample_defs/${f#*/}" ]] || ! cmp -s "${BUNDLE}/${f}" "${BUNDLE}/cfe/cmake/sample_defs/${f#*/}"; then
                defs+=("${f}")
            fi
        done < <(git -C "${BUNDLE}" ls-files "${c}_defs")
    done
    hits="$(cd "${BUNDLE}" && git grep -nE "${re}" \
            -- osal/src/os/freertos osal/src/bsp/freertos-posix-sim osal/src/bsp/freertos-arm-cm3 psp/fsw/freertos-posix-sim \
               psp/fsw/freertos-arm-cm3 psp/fsw/modules/timebase_freertos apps/counter_app ground_station README.md PORTING.md NOTICE \
               .gitignore Makefile "${defs[@]}" || true)"
    [[ -z "${hits}" ]] || { printf '%s\n' "${hits}"; die "private references in the committed bundle"; }
    if grep -nE "${re}" <<<"$(git -C "${BUNDLE}" log -1 --format=%B)"; then die "private references in the bundle commit message"; fi
    [[ -z "$(git -C "${BUNDLE}" ls-files | grep -E '^(docs|scripts|logs|tests|\.claude)/|^(CLAUDE|prompt)\.md$' || true)" ]] \
        || die "private files in the bundle"
    log "OK   no private references or private files in the committed bundle (files and commit message; ${#defs[@]} added/replaced *_defs files)"
}

# Builds configuration $1 in the clone with the stock commands into build folder $2.
build_config() {
    local config="$1" out="$2" blog="${LOG_DIR}/07-bundle-build-$1.log"
    log "building ${config} in a fresh clone (make MISSIONCONFIG=${config} O=${out} prep && make O=${out} && make O=${out} install)"
    (cd "${CLONE}" && make MISSIONCONFIG="${config}" O="${out}" prep && make O="${out}" -j"$(nproc)" && make O="${out}" install) >"${blog}" 2>&1 \
        || { tail -30 "${blog}"; die "bundle build of ${config} failed (see ${blog})"; }
    [[ -f "${CLONE}/${out}/exe/cpu1/core-cpu1" ]] || die "core-cpu1 not produced for ${config}"
    log "OK   ${config} built: ${out}/exe/cpu1/core-cpu1"
}

build_clone() {
    rm -rf "${CLONE}"
    mkdir -p "${CLONE%/*}"
    git clone -q "${BUNDLE}" "${CLONE}"
    build_config freertos_sim build-sim
    build_config freertos_qemu build-qemu
    [[ -z "$(git -C "${CLONE}" status --porcelain)" ]] || die "the builds left untracked or changed files in the clone: $(git -C "${CLONE}" status --porcelain | head -5)"
    log "OK   clone working tree still clean after both builds (cf.tar and build-*/ ignored)"
}

check_sim() {
    EXE_DIR="${CLONE}/build-sim/exe/cpu1"
    WORK_DIR="${CLONE}/build-sim"
    ARCH_SUBDIR="freertos-posix-sim"
    NM="nm"
    SIZE="size"
    RUN_DIR="${LOG_DIR}/07-bundle-runs"
    RUN_CMD="./core-cpu1"
    RUN_WARMUP_SECONDS=0
    # sch_lab runs in this configuration: its lines and counter_app's housekeeping line are expected (D44).
    COUNTER_CHECK_BUNDLE=0
    log "--- freertos_sim (Linux simulator)"
    cfe_sim_check_static_only
    check_cf_image "${CLONE}/freertos_sim_defs/cf.tar" cfe_es_startup.scr sch_lab_table.tbl to_lab_sub.tbl
    run_counter_app
    check_tctm
}

# The /cf image built by a single "make" in the clone holds exactly the given files (D44).
check_cf_image() {
    local tar="$1" want got
    shift
    [[ -f "${tar}" ]] || die "no /cf image ${tar}"
    want="$(printf '%s\n' "$@" | LC_ALL=C sort)"
    got="$(tar -tf "${tar}" | sed -n 's|^\./||p' | grep -v '^$' | grep -v '/$' | LC_ALL=C sort)"
    [[ "${got}" == "${want}" ]] || die "/cf image ${tar##*/}: has [$(echo ${got})], expected [$(echo ${want})]"
    log "OK   /cf image after one make: $(echo ${got})"
}

# Commands and telemetry over UDP in the clone, with the bundle's own ground station (D44): tlm-enable, noop,
# es-noop; counter_app housekeeping with the command counter 1, the NOOP event and ES's NOOP event.
check_tctm() {
    local dir="${LOG_DIR}/07-bundle-tctm" gs="${CLONE}/ground_station/ground_station.py" sim gsl t
    rm -rf "${dir}"
    mkdir -p "${dir}"
    for t in 1234 1235; do
        python3 -I -c 'import socket,sys; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.bind(("127.0.0.1",int(sys.argv[1])))' "${t}" 2>/dev/null \
            || die "UDP port ${t} is in use"
    done
    python3 -I "${gs}" listen --seconds 20 --log "${dir}/telemetry.log" >/dev/null 2>&1 &
    gsl=$!
    (cd "${CLONE}/build-sim/exe/cpu1" && exec ./core-cpu1 </dev/null >"${dir}/console.log" 2>&1) &
    sim=$!
    for t in $(seq 1 150); do
        grep -qF "CI_LAB listening on UDP port" "${dir}/console.log" 2>/dev/null && break
        sleep 0.1
    done
    sleep 1
    python3 -I "${gs}" send tlm-enable
    sleep 2
    python3 -I "${gs}" send noop
    sleep 2
    python3 -I "${gs}" send es-noop
    sleep 2
    kill -INT "${sim}" 2>/dev/null || true
    wait "${sim}" 2>/dev/null || true
    kill -TERM "${gsl}" 2>/dev/null || true   # background jobs ignore SIGINT in a non-interactive shell
    wait "${gsl}" 2>/dev/null || true
    grep -q "COUNTER_HK .* cmdcnt=1 errcnt=0" "${dir}/telemetry.log" || die "TC/TM: no counter_app housekeeping with command counter 1 (see ${dir})"
    grep -q "EVENT COUNTER_APP 10 INFO: NOOP command received" "${dir}/telemetry.log" || die "TC/TM: no NOOP event in the telemetry"
    grep -q "EVENT CFE_ES 3 INFO: No-op command" "${dir}/telemetry.log" || die "TC/TM: no ES NOOP event in the telemetry"
    log "OK   TC/TM with the bundle's ground station: housekeeping (command counter 1), NOOP event, ES NOOP event"
}

check_qemu() {
    # Cortex-M3 image checks (scripts/lib/cm3-image.sh sets the ARM binutils names) and QEMU runs.
    # shellcheck source=lib/cm3-image.sh
    source "${SCRIPT_DIR}/lib/cm3-image.sh"
    EXE_DIR="${CLONE}/build-qemu/exe/cpu1"
    WORK_DIR="${CLONE}/build-qemu"
    ARCH_SUBDIR="freertos-arm-cm3"
    MAP_COPY="${LOG_DIR}/07-bundle-qemu-core-cpu1.map"
    MALLOC_ALLOWED='^$'
    MALLOC_ALLOWED_TEXT="no object (none allowed on the target)"
    RUN_DIR="${LOG_DIR}/07-bundle-qemu-runs"
    RUN_CMD="${QEMU_RUN_CMD}"
    # The first QEMU launch right after the two builds took about 5 s to print anything (cold start), which
    # left only 14 of the 20 s for counting; one untimed warm-up launch first (limits unchanged).
    RUN_WARMUP_SECONDS=3
    # No sch_lab in this configuration: its lines are not expected.
    COUNTER_CHECK_BUNDLE=1
    log "--- freertos_qemu (Cortex-M3 on QEMU mps2-an385)"
    check_cf_image "${CLONE}/freertos_qemu_defs/cf.tar" cfe_es_startup.scr
    check_image
    check_got
    check_map
    check_no_heap
    cfe_sim_check_static_only
    run_counter_app
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
    check_cross_tools_and_qemu
    build_clone
    check_sim
    check_qemu

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 07-check-bundle OK ==="
}

main "$@"
