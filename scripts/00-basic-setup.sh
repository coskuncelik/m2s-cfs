#!/usr/bin/env bash
# 00-basic-setup.sh — set up and verify the pinned source submodules of m2s-cfs.
#
# Idempotent: a missing submodule is added (or initialized) at its pinned commit.
# A submodule that is already present is only verified, never moved.
# Any hash or version mismatch is a hard error.
#
# Usage: ./scripts/00-basic-setup.sh
# Env:   M2S_CFS_ROOT   override the repository root (default: parent of this script's directory)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/00-basic-setup.log"

# path | url | pinned commit
SUBMODULES=(
    "sources/cFS|https://github.com/nasa/cFS.git|c62b5b9c507af4972aad09ee9f3443fa16a9fc85"
    "sources/FreeRTOS-Kernel|https://github.com/FreeRTOS/FreeRTOS-Kernel.git|8a59a4b19bf72977819feddef87d41163ced7004"
    "sources/references/mission-v0|https://github.com/projetoChi2p/fs-nasa-cfs-mission-v0.git|ae4fe23e68735b8717ac0afeb0355fd863cd6ded"
    "sources/references/mission-v1|https://github.com/projetoChi2p/fs-nasa-cfs-mission-v1.git|01acb7ee267162786d86c1e75d3f60a83b760e16"
)

# path inside sources/cFS | pinned commit
CFS_NESTED=(
    "cfe|fc8c48ad09460ce0277ad2bfcd81d376634aa2c6"
    "osal|a77b5bd75d611670a6292f0a68922274d3dbf33f"
    "psp|ae0c0a77e9a6e67eeab4110e346bb29b1cf64fad"
    "apps/ci_lab|8c43b9eb45f2fe9ee5a8069cf07b2b45e3669971"
    "apps/sample_app|46279d0eeadf7cfa605888acc4db6c64193e4e88"
    "apps/sch_lab|dc58baf4f02e8adde0b411c6544c0f43a13de8d0"
    "apps/to_lab|fe6c9fd513f32c45615494554bafdbb646fcba6b"
    "libs/sample_lib|83d26957beaf256bbcdff3188a67dc4025726704"
    "tools/cFS-GroundSystem|c019d1bed5ffd9f6b3249bec37cc56b92bb36e0b"
    "tools/elf2cfetbl|fccae58fb587822a5e5c7a4de968de3d1dbb3ee9"
    "tools/tblCRCTool|42de9c416750e5c92575c74755fc4462a49a5b35"
)

# file (relative to root) | fixed-string pattern expected in it
VERSION_CHECKS=(
    "sources/cFS/cfe/modules/core_api/fsw/inc/cfe_version.h|#define CFE_BUILD_NUMBER   247"
    "sources/cFS/cfe/modules/core_api/fsw/inc/cfe_version.h|#define CFE_BUILD_BASELINE  \"equuleus-rc1\""
    "sources/cFS/osal/src/os/inc/osapi-version.h|#define OS_BUILD_NUMBER     117"
    "sources/cFS/osal/src/os/inc/osapi-version.h|#define OS_BUILD_BASELINE   \"equuleus-rc1\""
    "sources/cFS/psp/fsw/pc-linux/inc/psp_version.h|#define CFE_PSP_IMPL_BUILD_NUMBER   73"
    "sources/cFS/psp/fsw/pc-linux/inc/psp_version.h|#define CFE_PSP_IMPL_BUILD_BASELINE \"equuleus-rc1\""
    "sources/FreeRTOS-Kernel/include/task.h|#define tskKERNEL_VERSION_NUMBER                      \"V11.3.2\""
)

ERRORS=0

log()  { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"; }
fail() { log "ERROR: $*"; ERRORS=$((ERRORS + 1)); }
die()  { log "FATAL: $*"; exit 1; }

check_prerequisites() {
    command -v git >/dev/null 2>&1 || die "git not found in PATH"
    [[ -f "${ROOT_DIR}/CLAUDE.md" ]] || die "${ROOT_DIR} does not look like the m2s-cfs root (CLAUDE.md missing)"
    log "git: $(git --version)"
    log "root: ${ROOT_DIR}"
}

ensure_repo() {
    if [[ -d "${ROOT_DIR}/.git" ]]; then
        log "git repository present"
    else
        log "initializing git repository (branch main)"
        git -C "${ROOT_DIR}" init -q -b main
    fi
}

# True if <path> is registered in .gitmodules.
is_registered() {
    git -C "${ROOT_DIR}" config -f .gitmodules --get "submodule.$1.path" >/dev/null 2>&1
}

ensure_submodule() {
    local path="$1" url="$2" commit="$3"
    if ! is_registered "${path}"; then
        log "adding ${path} <- ${url}"
        git -C "${ROOT_DIR}" submodule add -q "${url}" "${path}"
        git -C "${ROOT_DIR}/${path}" checkout -q --detach "${commit}"
        git -C "${ROOT_DIR}" add "${path}"
    elif [[ ! -e "${ROOT_DIR}/${path}/.git" ]]; then
        log "initializing registered submodule ${path}"
        git -C "${ROOT_DIR}" submodule update -q --init "${path}"
    else
        log "${path} already present (verify only)"
    fi
}

verify_head() {
    local dir="$1" expected="$2" label="$3" actual
    actual="$(git -C "${dir}" rev-parse HEAD 2>/dev/null)" || { fail "${label}: not a git checkout (${dir})"; return; }
    if [[ "${actual}" == "${expected}" ]]; then
        log "OK   ${label} ${actual}"
    else
        fail "${label}: HEAD ${actual}, expected ${expected}"
    fi
}

verify_gitlink() {
    local repo="$1" path="$2" expected="$3" label="$4" actual
    actual="$(git -C "${repo}" ls-files -s -- "${path}" | awk '$1 == "160000" {print $2}')"
    if [[ "${actual}" == "${expected}" ]]; then
        log "OK   ${label} gitlink ${actual}"
    else
        fail "${label}: recorded gitlink '${actual}', expected ${expected}"
    fi
}

main() {
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 00-basic-setup start ==="

    check_prerequisites
    ensure_repo

    local entry path url commit
    for entry in "${SUBMODULES[@]}"; do
        IFS='|' read -r path url commit <<<"${entry}"
        ensure_submodule "${path}" "${url}" "${commit}"
    done

    # cFS's own submodules come from its pinned gitlinks (no remote tracking).
    log "updating sources/cFS nested submodules (recursive, pinned by cFS)"
    git -C "${ROOT_DIR}/sources/cFS" submodule update -q --init --recursive

    log "--- verifying revisions ---"
    for entry in "${SUBMODULES[@]}"; do
        IFS='|' read -r path url commit <<<"${entry}"
        verify_gitlink "${ROOT_DIR}" "${path}" "${commit}" "${path}"
        verify_head "${ROOT_DIR}/${path}" "${commit}" "${path}"
    done
    local sub
    for entry in "${CFS_NESTED[@]}"; do
        IFS='|' read -r sub commit <<<"${entry}"
        verify_gitlink "${ROOT_DIR}/sources/cFS" "${sub}" "${commit}" "sources/cFS/${sub}"
        verify_head "${ROOT_DIR}/sources/cFS/${sub}" "${commit}" "sources/cFS/${sub}"
    done

    log "--- verifying version strings ---"
    local file pattern
    for entry in "${VERSION_CHECKS[@]}"; do
        IFS='|' read -r file pattern <<<"${entry}"
        if grep -qF -- "${pattern}" "${ROOT_DIR}/${file}" 2>/dev/null; then
            log "OK   ${file}: ${pattern}"
        else
            fail "${file}: missing '${pattern}'"
        fi
    done

    if (( ERRORS > 0 )); then
        log "=== 00-basic-setup FAILED: ${ERRORS} error(s) ==="
        exit 1
    fi
    log "=== 00-basic-setup OK ==="
}

main "$@"
