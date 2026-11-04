#!/usr/bin/env bash
# 01-check-prerequisites.sh — read-only check of host tools needed by m2s-cfs.
#
# Never installs anything. It reports missing tools and prints a suggested
# apt command for the user to run.
#
#   REQUIRED : needed for the current phases (native-POSIX baseline, FreeRTOS Linux simulator).
#              Any missing or too-old REQUIRED tool -> exit 1.
#   FUTURE   : needed later for the SmartFusion2 hardware phases. Reported only, never fails.
#
# Usage: ./scripts/01-check-prerequisites.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${M2S_CFS_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
LOG_DIR="${ROOT_DIR}/logs"
LOG_FILE="${LOG_DIR}/01-check-prerequisites.log"

# cFE/OSAL top-level CMakeLists.txt require at least this version.
CMAKE_MIN_VERSION="3.5"

# tool | apt package | purpose
REQUIRED=(
    "git|git|source control and submodules"
    "make|make|cFE Makefile wrapper"
    "gcc|gcc|host compiler (native build and simulator)"
    "cmake|cmake|cFE/OSAL/PSP build system"
    "xxd|xxd|cFE EMBED_FILELIST (embedded startup script)"
)
FUTURE=(
    "arm-none-eabi-gcc|gcc-arm-none-eabi|Cortex-M3 cross compiler"
    "arm-none-eabi-objcopy|binutils-arm-none-eabi|image conversion"
    "gdb-multiarch|gdb-multiarch|JTAG debugging (or arm-none-eabi-gdb)"
    "openocd|openocd|JTAG server (Microchip build may be needed for FlashPro)"
)

MISSING_REQUIRED=()
MISSING_FUTURE=()

log() { printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"; }

first_line_version() {
    # First line of "<tool> --version". A failure is not fatal.
    "$1" --version 2>&1 | head -n 1 || true
}

# Returns 0 if version $1 >= version $2 (dotted numeric).
version_ge() {
    [[ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n 1)" == "$2" ]]
}

check_tool() {
    local level="$1" tool="$2" pkg="$3" purpose="$4" path
    if path="$(command -v "${tool}")"; then
        log "OK      [${level}] ${tool} -> ${path} ($(first_line_version "${tool}"))"
        return 0
    fi
    log "MISSING [${level}] ${tool} — ${purpose} (apt: ${pkg})"
    return 1
}

check_cmake_version() {
    command -v cmake >/dev/null 2>&1 || return 0   # absence already reported
    local ver
    ver="$(cmake --version | head -n 1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' || true)"
    if [[ -z "${ver}" ]]; then
        log "ERROR   cannot parse cmake version"
        MISSING_REQUIRED+=("cmake")
    elif ! version_ge "${ver}" "${CMAKE_MIN_VERSION}"; then
        log "ERROR   cmake ${ver} < required ${CMAKE_MIN_VERSION}"
        MISSING_REQUIRED+=("cmake")
    else
        log "OK      cmake ${ver} >= ${CMAKE_MIN_VERSION}"
    fi
}

main() {
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 01-check-prerequisites start ==="
    log "host: $(. /etc/os-release 2>/dev/null && echo "${PRETTY_NAME:-unknown}") / $(uname -m)"

    local entry tool pkg purpose
    for entry in "${REQUIRED[@]}"; do
        IFS='|' read -r tool pkg purpose <<<"${entry}"
        check_tool "REQUIRED" "${tool}" "${pkg}" "${purpose}" || MISSING_REQUIRED+=("${pkg}")
    done
    check_cmake_version

    for entry in "${FUTURE[@]}"; do
        IFS='|' read -r tool pkg purpose <<<"${entry}"
        check_tool "FUTURE" "${tool}" "${pkg}" "${purpose}" || MISSING_FUTURE+=("${pkg}")
    done

    if (( ${#MISSING_FUTURE[@]} > 0 )); then
        log "NOTE    FUTURE tools missing (not needed yet; toolchain choice still to be decided):"
        log "        sudo apt install ${MISSING_FUTURE[*]}"
    fi

    if (( ${#MISSING_REQUIRED[@]} > 0 )); then
        # Deduplicate, keeping order.
        local uniq
        uniq="$(printf '%s\n' "${MISSING_REQUIRED[@]}" | awk '!seen[$0]++' | tr '\n' ' ')"
        log "ACTION  install the missing REQUIRED packages yourself, then re-run this script:"
        log "        sudo apt install ${uniq% }"
        log "=== 01-check-prerequisites FAILED ==="
        exit 1
    fi
    log "=== 01-check-prerequisites OK ==="
}

main "$@"
