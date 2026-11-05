#!/usr/bin/env bash
# 07-export-bundle.sh — generate the shareable bundle "cfs-m2s-freertos" (step 07, D30).
#
# The bundle has the layout of NASA's cFS bundle (nasa/cFS) with the FreeRTOS port added where cFE and OSAL
# look for platforms, so it builds with the stock "make prep && make && make install". It is written to
# build/export/cfs-m2s-freertos as a new git repository with one commit (no history of this repository).
#
#   upstream : sources/cFS (with its submodules) and sources/FreeRTOS-Kernel, copied file by file from the
#              clean, pinned working trees. Changed: root .gitignore (replaced), root README.md (renamed
#              README-cFS.md); left out: root .github/ (NASA CI) and root .gitmodules (code is copied in).
#   added    : osal-ext/freertos           -> osal/src/os/freertos
#              osal-ext/freertos-posix-sim -> osal/src/bsp/freertos-posix-sim
#                + cmake/freertos-posix-kernel.cmake -> .../cmake/, config/freertos/posix/* -> .../config/,
#                  patches/freertos-kernel/*.patch -> .../patches/
#              psp-ext/fsw/freertos-posix-sim, psp-ext/fsw/modules/timebase_freertos -> psp/fsw/...
#              apps/counter_app -> apps/counter_app
#              Makefile (= cfe/cmake/Makefile.sample), sample_defs/ (= cfe/cmake/sample_defs + export/sample_defs,
#              mission_build_custom.cmake + export/mission_build_custom.append.cmake), README.md, PORTING.md, NOTICE
#   cleanup  : references to this repository's private documents, steps and scripts are removed from the
#              added files (exact replacements, then generic rules); the export fails if any remain.
#
# Idempotent: the output directory is rebuilt from scratch; the same inputs give the same tree.
#
# Usage: ./scripts/07-export-bundle.sh            (env: EXPORT_DIR, default build/export)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUNDLE_NAME="cfs-m2s-freertos"
EXPORT_DIR="${EXPORT_DIR:-${ROOT_DIR}/build/export}"
OUT_DIR="${EXPORT_DIR}/${BUNDLE_NAME}"
CFS_DIR="${ROOT_DIR}/sources/cFS"
KERNEL_DIR="${ROOT_DIR}/sources/FreeRTOS-Kernel"
EXPORT_SRC="${ROOT_DIR}/export"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/07-export-bundle.log"

# Project paths exported (also used for the "-dirty" mark of the source revision).
PROJECT_PATHS=(osal-ext psp-ext apps/counter_app cmake/freertos-posix-kernel.cmake config/freertos/posix
               patches/freertos-kernel export)

# Added directories in the bundle (relative to OUT_DIR), for PORTING.md and the checks.
ADDED_DIRS=(osal/src/os/freertos osal/src/bsp/freertos-posix-sim psp/fsw/freertos-posix-sim
            psp/fsw/modules/timebase_freertos apps/counter_app)

# Private references that must not appear in any added or generated file (extended regex).
LEAK_PATTERN='\bD[0-9]{1,2}\b|DECISIONS|STATUS\.md|prompt\.md|CLAUDE|[Ss]teps? 0[0-9]|\b0[0-9][abc]\b|m2s-cfs|scripts/|osal-ext|psp-ext|OSAL_EXT_SOURCE_DIR|sources/|config/(cfe|osal|freertos)/|patches/freertos-kernel|docs/|tests/|test-results|/workspaces|codespace'

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"; }
die() { log "FATAL: $*"; exit 1; }

check_prerequisites() {
    local tool
    for tool in git tar python3 sed cmp; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found"
    done
    [[ -f "${CFS_DIR}/cfe/CMakeLists.txt" && -f "${KERNEL_DIR}/tasks.c" ]] \
        || die "sources/ not populated — run scripts/00-basic-setup.sh"
    [[ -z "$(git -C "${CFS_DIR}" status --porcelain --ignore-submodules=none)" ]] || die "sources/cFS is not clean"
    [[ -z "$(git -C "${KERNEL_DIR}" status --porcelain --ignore-submodules=none)" ]] || die "sources/FreeRTOS-Kernel is not clean"
    [[ -d "${EXPORT_SRC}/sample_defs" ]] || die "export/ not found"
}

# Copies the files git tracks in $1 (with populated submodules) to $2, keeping modes and symlinks.
copy_tracked() {
    local src="$1" dst="$2"
    mkdir -p "${dst}"
    (cd "${src}" && git ls-files -z --recurse-submodules | while IFS= read -r -d '' f; do
         [[ -f "${f}" || -L "${f}" ]] && printf '%s\0' "${f}"
     done) | tar --null -C "${src}" -T - -cf - | tar -C "${dst}" -xf -
}

# Copies the project files (tracked or new, not ignored) under $1 to $2.
copy_project() {
    local src="$1" dst="$2"
    [[ ! -e "${OUT_DIR}/${dst}" ]] || die "${dst} already exists upstream"
    mkdir -p "${OUT_DIR}/${dst}"
    (cd "${ROOT_DIR}/${src}" && git ls-files -z --cached --others --exclude-standard . | while IFS= read -r -d '' f; do
         [[ -f "${f}" ]] && printf '%s\0' "${f}"
     done) | tar --null -C "${ROOT_DIR}/${src}" -T - -cf - | tar -C "${OUT_DIR}/${dst}" -xf -
}

copy_upstream() {
    copy_tracked "${CFS_DIR}" "${OUT_DIR}"
    rm -rf "${OUT_DIR}/.github" "${OUT_DIR}/.gitmodules"
    mv "${OUT_DIR}/README.md" "${OUT_DIR}/README-cFS.md"
    copy_tracked "${KERNEL_DIR}" "${OUT_DIR}/FreeRTOS-Kernel"
    log "OK   upstream copied: $(cd "${CFS_DIR}" && git ls-files --recurse-submodules | wc -l) cFS paths, $(cd "${KERNEL_DIR}" && git ls-files | wc -l) FreeRTOS-Kernel paths"
}

add_port() {
    local bsp="osal/src/bsp/freertos-posix-sim"
    copy_project osal-ext/freertos osal/src/os/freertos
    copy_project osal-ext/freertos-posix-sim "${bsp}"
    mkdir -p "${OUT_DIR}/${bsp}/cmake" "${OUT_DIR}/${bsp}/config" "${OUT_DIR}/${bsp}/patches"
    cp "${ROOT_DIR}/cmake/freertos-posix-kernel.cmake" "${OUT_DIR}/${bsp}/cmake/"
    cp "${ROOT_DIR}/config/freertos/posix/FreeRTOSConfig.h" "${ROOT_DIR}/config/freertos/posix/freertos_posix_hooks.c" \
       "${OUT_DIR}/${bsp}/config/"
    cp "${ROOT_DIR}/patches/freertos-kernel/0001-posix-port-tick-catch-up.patch" "${OUT_DIR}/${bsp}/patches/"
    copy_project psp-ext/fsw/freertos-posix-sim psp/fsw/freertos-posix-sim
    copy_project psp-ext/fsw/modules/timebase_freertos psp/fsw/modules/timebase_freertos
    copy_project apps/counter_app apps/counter_app

    cp "${OUT_DIR}/cfe/cmake/Makefile.sample" "${OUT_DIR}/Makefile"
    cp -a "${OUT_DIR}/cfe/cmake/sample_defs" "${OUT_DIR}/sample_defs"
    cp "${EXPORT_SRC}"/sample_defs/* "${OUT_DIR}/sample_defs/"
    cat "${EXPORT_SRC}/mission_build_custom.append.cmake" >>"${OUT_DIR}/sample_defs/mission_build_custom.cmake"
    cp "${EXPORT_SRC}/README.md" "${EXPORT_SRC}/PORTING.md" "${EXPORT_SRC}/NOTICE" "${OUT_DIR}/"
    cp "${EXPORT_SRC}/gitignore" "${OUT_DIR}/.gitignore"
    find "${OUT_DIR}/osal/src/os/freertos" "${OUT_DIR}/${bsp}" "${OUT_DIR}/psp/fsw/freertos-posix-sim" \
         "${OUT_DIR}/psp/fsw/modules/timebase_freertos" "${OUT_DIR}/apps/counter_app" "${OUT_DIR}/sample_defs" \
         -type f -exec chmod 0644 {} +
    log "OK   port, mission config and bundle files added"
}

# Exact replacements (each must match exactly once), then generic rules, on the added files.
clean_references() {
    python3 - "${OUT_DIR}" <<'PYEOF'
import os, re, sys
out = sys.argv[1]
B = "osal/src/bsp/freertos-posix-sim/"
O = "osal/src/os/freertos/"
P = "psp/fsw/freertos-posix-sim/"
exact = [
    (B + "CMakeLists.txt",
     "# Found by OSAL through OSAL_EXT_SOURCE_DIR. Pairs with OSAL_SYSTEM_OSTYPE=freertos.",
     "# Found by OSAL in osal/src/bsp (OSAL_SYSTEM_BSPTYPE). Pairs with OSAL_SYSTEM_OSTYPE=freertos."),
    (B + "CMakeLists.txt",
     "# Kernel + patched POSIX port + project hooks (shared with tests/freertos-posix-smoke).\n"
     "include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/freertos-posix-kernel.cmake)\n",
     "# Kernel + patched POSIX port + hooks. The kernel tree defaults to FreeRTOS-Kernel/ at the top of the\n"
     "# bundle; set the cache variable M2S_FREERTOS_DIR to use another one.\n"
     "get_filename_component(M2S_FREERTOS_DEFAULT_DIR \"${CMAKE_CURRENT_LIST_DIR}/../../../../FreeRTOS-Kernel\" ABSOLUTE)\n"
     "set(M2S_FREERTOS_DIR \"${M2S_FREERTOS_DEFAULT_DIR}\" CACHE PATH \"FreeRTOS-Kernel source tree\")\n"
     "set(M2S_FREERTOS_CONFIG_DIR \"${CMAKE_CURRENT_LIST_DIR}/config\")\n"
     "set(M2S_FREERTOS_PORT_PATCH \"${CMAKE_CURRENT_LIST_DIR}/patches/0001-posix-port-tick-catch-up.patch\")\n"
     "include(${CMAKE_CURRENT_LIST_DIR}/cmake/freertos-posix-kernel.cmake)\n"),
    (B + "cmake/freertos-posix-kernel.cmake",
     "# (patches/freertos-kernel/0001-posix-port-tick-catch-up.patch, applied with `patch -o` into the\n"
     "# build directory; sources/ is never modified)",
     "# (patches/0001-posix-port-tick-catch-up.patch, applied with `patch -o` into the\n"
     "# build directory; the kernel tree is never modified)"),
    (B + "cmake/freertos-posix-kernel.cmake",
     "may be set before including this file; the defaults are this repository's layout.\n\n"
     "get_filename_component(M2S_ROOT \"${CMAKE_CURRENT_LIST_DIR}/..\" ABSOLUTE)\n"
     "if(NOT DEFINED M2S_FREERTOS_DIR)\n"
     "    set(M2S_FREERTOS_DIR \"${M2S_ROOT}/sources/FreeRTOS-Kernel\")\n"
     "endif()\n"
     "if(NOT DEFINED M2S_FREERTOS_CONFIG_DIR)\n"
     "    set(M2S_FREERTOS_CONFIG_DIR \"${M2S_ROOT}/config/freertos/posix\")\n"
     "endif()\n"
     "if(NOT DEFINED M2S_FREERTOS_PORT_PATCH)\n"
     "    set(M2S_FREERTOS_PORT_PATCH \"${M2S_ROOT}/patches/freertos-kernel/0001-posix-port-tick-catch-up.patch\")\n"
     "endif()\n",
     "must be set before including this file (the BSP's CMakeLists.txt sets them).\n\n"
     "foreach(m2s_var M2S_FREERTOS_DIR M2S_FREERTOS_CONFIG_DIR M2S_FREERTOS_PORT_PATCH)\n"
     "    if(NOT DEFINED ${m2s_var})\n"
     "        message(FATAL_ERROR \"${m2s_var} must be set before including freertos-posix-kernel.cmake\")\n"
     "    endif()\n"
     "endforeach()\n"),
    (O + "CMakeLists.txt",
     "# Found by OSAL through OSAL_EXT_SOURCE_DIR (see docs/DECISIONS.md D17).\n",
     "# Found by OSAL in osal/src/os (OSAL_SYSTEM_OSTYPE).\n"),
    (O + "CMakeLists.txt", "# static RAM file system (04b)", "# static RAM file system"),
    (O + "inc/os-freertos.h", " * See docs/DECISIONS.md D17-D19.\n", ""),
    (O + "inc/os-freertos.h", "(os-impl-filesys.c, step 04b):", "(os-impl-filesys.c):"),
    (O + "inc/os-ramfs.h", " core (step 04b, docs/DECISIONS.md D19, D25).", " core."),
    (O + "src/os-impl-filesys.c", " Step 04b, docs/DECISIONS.md D19, D25.", ""),
    (B + "src/bsp_start.c", "(measured in step 04a; see docs/DECISIONS.md D21)", "(measured on a 2-vCPU VM)"),
    (B + "config/freertos_posix_hooks.c", " See docs/DECISIONS.md D15.", ""),
    (B + "config/FreeRTOSConfig.h", "(osal-ext/freertos)", "(osal/src/os/freertos)"),
    (B + "patches/0001-posix-port-tick-catch-up.patch",
     "Problem (measured 2026-11-03, see docs/test-results/03-freertos-posix-smoke.md):",
     "Problem (measured 2026-11-03 on a 2-vCPU VirtualBox VM):"),
    (B + "patches/0001-posix-port-tick-catch-up.patch",
     "Apply (never in sources/): patch -o <build>/port.c sources/FreeRTOS-Kernel/portable/",
     "Apply (the kernel tree is never modified): patch -o <build>/port.c FreeRTOS-Kernel/portable/"),
    (P + "psp_module_list.cmake", "(psp-ext/fsw/modules)", "(added in fsw/modules)"),
    (P + "CMakeLists.txt",
     "# Built from a build-time copy of the stock PSP tree (scripts/05, D27); sources/ is never written.\n", ""),
    (P + "src/cfe_psp_start.c", "The OSAL BSP (osal-ext/freertos-posix-sim)", "The OSAL BSP (osal/src/bsp/freertos-posix-sim)"),
    (P + "src/cfe_psp_start.c", "built by the mission scripts", "built by the mission configuration"),
    (P + "inc/psp_version.h", "(equuleus-rc1+dev73, docs/VERSIONS.md, D1)", "(equuleus-rc1+dev73)"),
]
generic = [
    (re.compile(r"m2s-cfs"), "cfs-m2s-freertos"),
    (re.compile(r" ?\((?:see )?(?:docs/DECISIONS\.md )?D\d+(?:(?:, ?|-)D\d+)*\)"), ""),
    (re.compile(r", D\d+\)"), ")"),
]
dirs = [O, B, P, "psp/fsw/modules/timebase_freertos/", "apps/counter_app/"]
for path, old, new in exact:
    f = os.path.join(out, path)
    s = open(f, encoding="utf-8").read()
    n = s.count(old)
    if n != 1:
        sys.exit("exact replacement matched %d times in %s: %r" % (n, path, old[:60]))
    open(f, "w", encoding="utf-8").write(s.replace(old, new))
changed = 0
for d in dirs:
    for base, _, files in os.walk(os.path.join(out, d)):
        for name in files:
            f = os.path.join(base, name)
            s = open(f, encoding="utf-8").read()
            t = s
            for rx, rep in generic:
                t = rx.sub(rep, t)
            if t != s:
                open(f, "w", encoding="utf-8").write(t)
                changed += 1
print("%d exact replacements, generic rules changed %d files" % (len(exact), changed))
PYEOF
}

# Appends the generated sections to PORTING.md.
write_porting_lists() {
    local porting="${OUT_DIR}/PORTING.md" rev dirty="" d f
    rev="$(git -C "${ROOT_DIR}" rev-parse --short=12 HEAD)"
    [[ -z "$(git -C "${ROOT_DIR}" status --porcelain -- "${PROJECT_PATHS[@]}")" ]] || dirty="-dirty"
    {
        printf '\n## Pinned versions (generated)\n\n'
        printf '| Component | Commit | Version |\n|---|---|---|\n'
        printf '| cFS bundle (`nasa/cFS`) | `%s` | `%s` |\n' "$(git -C "${CFS_DIR}" rev-parse HEAD)" "$(git -C "${CFS_DIR}" describe --tags --always)"
        git -C "${CFS_DIR}" submodule status | while read -r sha path desc; do
            printf '| `%s` | `%s` | `%s` |\n' "${path}" "${sha#[-+ ]}" "${desc//[()]/}"
        done
        printf '| `FreeRTOS-Kernel` | `%s` | `%s` |\n' "$(git -C "${KERNEL_DIR}" rev-parse HEAD)" "$(git -C "${KERNEL_DIR}" describe --tags --always)"
        printf '\nExported from source revision `%s%s`.\n' "${rev}" "${dirty}"

        printf '\n## Added files (generated)\n\n```\n'
        for d in "${ADDED_DIRS[@]}"; do
            (cd "${OUT_DIR}" && find "${d}" -type f | LC_ALL=C sort)
        done
        printf 'Makefile                        (= cfe/cmake/Makefile.sample)\n'
        printf 'README.md  PORTING.md  NOTICE\n'
        printf 'sample_defs/                    (= cfe/cmake/sample_defs, with these files added or replaced:)\n'
        (cd "${OUT_DIR}/sample_defs" && for f in $(LC_ALL=C ls); do
             [[ -f "${f}" ]] || continue
             if [[ ! -e "${OUT_DIR}/cfe/cmake/sample_defs/${f}" ]]; then
                 printf '  sample_defs/%s (added)\n' "${f}"
             elif ! cmp -s "${f}" "${OUT_DIR}/cfe/cmake/sample_defs/${f}"; then
                 printf '  sample_defs/%s (replaced%s)\n' "${f}" "$([[ "${f}" == mission_build_custom.cmake ]] && echo '; stock content kept, additions appended')"
             fi
         done)
        printf '```\n\n## Changed upstream files (generated)\n\n'
        printf -- '- `.gitignore`: replaced (the stock file ignores `Makefile` and `/sample_defs`, which this bundle ships).\n'
        printf -- '- `README.md`: NASA'"'"'s file renamed to `README-cFS.md`, content unchanged.\n'
        printf -- '- `.github/` (NASA CI workflows) and `.gitmodules` (the components are copied in, not submodules): left out.\n'
        printf -- '- Every other file of the upstream trees is byte-identical to the commits above.\n'
    } >>"${porting}"
}

check_leaks() {
    local hits
    hits="$(cd "${OUT_DIR}" && grep -rnE -- "${LEAK_PATTERN}" "${ADDED_DIRS[@]}" sample_defs Makefile README.md PORTING.md NOTICE .gitignore \
            | grep -vE '^sample_defs/[^:]*:' || true)"
    # sample_defs: only the files this bundle adds or replaces are checked (the stock ones are upstream).
    local f
    for f in "${OUT_DIR}"/sample_defs/*; do
        [[ -f "${f}" ]] || continue
        if [[ ! -e "${OUT_DIR}/cfe/cmake/sample_defs/${f##*/}" ]] || ! cmp -s "${f}" "${OUT_DIR}/cfe/cmake/sample_defs/${f##*/}"; then
            hits+="$(grep -nE -- "${LEAK_PATTERN}" "${f}" | sed "s|^|sample_defs/${f##*/}:|" || true)"
        fi
    done
    if [[ -n "${hits}" ]]; then
        printf '%s\n' "${hits}" | sed 's/^/        /'
        die "private references left in the bundle"
    fi
    log "OK   no private references in added or generated files"
}

make_repo() {
    git -C "${OUT_DIR}" init -q -b main
    git -C "${OUT_DIR}" add -A
    [[ -z "$(git -C "${OUT_DIR}" status --porcelain --ignored | grep -v '^A ' || true)" ]] \
        || die "files in the bundle are ignored by its .gitignore: $(git -C "${OUT_DIR}" status --porcelain --ignored | grep -v '^A ' | head -5)"
    git -C "${OUT_DIR}" commit -q -m "cfs-m2s-freertos: cFS on FreeRTOS V11.3.2 (Linux simulator)" \
        -m "Initial export. Versions and the list of added and changed files: PORTING.md."
    log "OK   bundle repository: ${OUT_DIR} ($(git -C "${OUT_DIR}" ls-files | wc -l) files, tree $(git -C "${OUT_DIR}" rev-parse --short=12 'HEAD^{tree}'))"
}

main() {
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 07-export-bundle start (${OUT_DIR}) ==="
    check_prerequisites
    rm -rf "${OUT_DIR}"
    mkdir -p "${OUT_DIR}"
    copy_upstream
    add_port
    local msg
    msg="$(clean_references)"
    log "OK   ${msg}"
    write_porting_lists
    check_leaks
    make_repo
    log "=== 07-export-bundle OK ==="
}

main "$@"
