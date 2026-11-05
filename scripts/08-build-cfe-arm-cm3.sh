#!/usr/bin/env bash
# 08-build-cfe-arm-cm3.sh — cross-build cFE on FreeRTOS for ARM Cortex-M3 and check the image (step 08, D31).
#
# Build and link only; nothing runs (an emulator run is step 09, the board later). Mission
# config/cfe/freertos-arm-cm3: the same cFE, OSAL FreeRTOS layer, PSP sources and counter_app as the
# simulator, built with arm-none-eabi-gcc (newlib-nano) against the FreeRTOS GCC/ARM_CM3 port, with the
# OSAL BSP osal-ext/freertos-arm-cm3, the PSP psp-ext/fsw/freertos-arm-cm3 and the PLACEHOLDER board
# boards/link-check (vector table, linker script, newlib stubs with the _sbrk trap).
#
# Checks:
#   1. configure + build + install with cFE's strict flags (-Werror; project code also -Wextra);
#   2. core-cpu1 is an ELF32 ARM EABI5 image for an ARMv7-M (Cortex-M3) Thumb target; the entry point is
#      Reset_Handler; vector table entries 0 (initial stack), 1 (reset), 11 (SVCall), 14 (PendSV) and
#      15 (SysTick) point to _estack, Reset_Handler and the FreeRTOS port handlers;
#   3. the link map exists (copied to logs/);
#   4. no dynamic allocation: newlib's allocator objects are not linked; malloc/free/calloc/realloc, their
#      _r forms and _sbrk exist only as the board's traps (syscalls.c, per the link map); no FreeRTOS heap
#      or dynamic FreeRTOS object creation; no target object references malloc/free;
#   5. footprint report: sections, the 20 largest RAM symbols, comparison with the simulator build;
#   6. sources/ unchanged.
#
# Usage: ./scripts/08-build-cfe-arm-cm3.sh [--clean]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/cfe-sim.sh
source "${SCRIPT_DIR}/lib/cfe-sim.sh"
cfe_sim_init freertos-arm-cm3 freertos-arm-cm3
LOG_FILE="${LOG_DIR}/08-build-cfe-arm-cm3.log"
REPORT="${LOG_DIR}/08-footprint.txt"
MAP_COPY="${LOG_DIR}/08-core-cpu1.map"

NM="arm-none-eabi-nm"
SIZE="arm-none-eabi-size"
READELF="arm-none-eabi-readelf"
OBJDUMP="arm-none-eabi-objdump"
MALLOC_ALLOWED='^$'                    # no target object may reference malloc/free
MALLOC_ALLOWED_TEXT="no object (none allowed on the target)"
SIM_BIN="${ROOT_DIR}/build/cfe-freertos-sim/exe/cpu1/core-cpu1"

DO_CLEAN=0

parse_args() {
    local arg
    for arg in "$@"; do
        case "${arg}" in
            --clean) DO_CLEAN=1 ;;
            *) die "unknown argument '${arg}' (usage: $0 [--clean])" ;;
        esac
    done
}

check_cross_tools() {
    local tool spec
    for tool in arm-none-eabi-gcc "${NM}" "${SIZE}" "${READELF}" "${OBJDUMP}"; do
        command -v "${tool}" >/dev/null 2>&1 || die "'${tool}' not found — run scripts/01-check-prerequisites.sh for the install command"
    done
    spec="$(arm-none-eabi-gcc -print-file-name=nano.specs)"
    [[ "${spec}" == /* && -f "${spec}" ]] || die "newlib-nano not found — run scripts/01-check-prerequisites.sh"
    log "OK   cross toolchain: $(arm-none-eabi-gcc --version | head -1); newlib-nano ${spec}"
}

# Address of a symbol (hex, no 0x, lower case); empty if absent.
sym_addr() {
    awk -v s="$2" '$3 == s { print $1; exit }' <<<"$1"
}

check_image() {
    local bin="${EXE_DIR}/core-cpu1" hdr attrs syms entry reset words i name want got bad=0
    hdr="$("${READELF}" -h "${bin}")"
    grep -Eq 'Class:[[:space:]]+ELF32' <<<"${hdr}" || die "core-cpu1 is not ELF32"
    grep -Eq 'Machine:[[:space:]]+ARM' <<<"${hdr}" || die "core-cpu1 is not an ARM image"
    grep -Eq 'Flags:.*Version5 EABI' <<<"${hdr}" || die "core-cpu1 is not EABI version 5"
    attrs="$("${READELF}" -A "${bin}")"
    grep -Eq "Tag_CPU_arch: v7" <<<"${attrs}" || die "core-cpu1 is not built for ARMv7"
    grep -Eq "Tag_CPU_arch_profile: Microcontroller" <<<"${attrs}" || die "core-cpu1 is not built for an M-profile CPU"
    grep -Eq "Tag_THUMB_ISA_use: Thumb-2" <<<"${attrs}" || die "core-cpu1 does not use Thumb-2"
    grep -Eq "Tag_ARM_ISA_use" <<<"${attrs}" && die "core-cpu1 contains ARM (non-Thumb) code, which a Cortex-M3 cannot run"

    syms="$("${NM}" "${bin}")"
    entry="$(sed -nE 's/.*Entry point address:[[:space:]]+0x([0-9a-f]+).*/\1/p' <<<"${hdr}")"
    reset="$(sym_addr "${syms}" Reset_Handler)"
    [[ -n "${reset}" ]] || die "Reset_Handler not in core-cpu1"
    (( (16#${entry} & ~1) == 16#${reset} )) || die "entry point 0x${entry} is not Reset_Handler (0x${reset})"

    # Vector table: first 16 words of .isr_vector (little endian), Thumb bit cleared for code addresses.
    mapfile -t words < <("${OBJDUMP}" -s -j .isr_vector "${bin}" | awk '/^ [0-9a-f]+ / { for (i = 2; i <= 5; i++) print $i }' \
                         | head -16 | sed -E 's/(..)(..)(..)(..)/\4\3\2\1/')
    (( ${#words[@]} == 16 )) || die "vector table not found (.isr_vector)"
    for i in 0 1 11 14 15; do
        case "${i}" in
            0) name=_estack ;; 1) name=Reset_Handler ;; 11) name=vPortSVCHandler ;;
            14) name=xPortPendSVHandler ;; 15) name=xPortSysTickHandler ;;
        esac
        want="$(sym_addr "${syms}" "${name}")"
        got=$(( 16#${words[i]} & ( i == 0 ? ~0 : ~1 ) ))
        if [[ -z "${want}" ]] || (( got != 16#${want} )); then
            log "ERROR vector ${i}: 0x${words[i]}, expected ${name} (0x${want:-absent})"
            bad=1
        fi
    done
    (( bad == 0 )) || die "vector table does not route to the expected handlers"
    log "OK   core-cpu1: ELF32 ARM EABI5, ARMv7-M Thumb-2; entry = Reset_Handler (0x${reset});"
    log "     vectors: SP = _estack, reset, SVCall/PendSV/SysTick -> FreeRTOS ARM_CM3 port handlers"
}

check_map() {
    local map
    map="$(find "${WORK_DIR}/${ARCH_SUBDIR}" -name core-cpu1.map | head -1)"
    [[ -n "${map}" && -s "${map}" ]] || die "link map core-cpu1.map not found"
    cp "${map}" "${MAP_COPY}"
    log "OK   link map: ${MAP_COPY} ($(wc -l <"${MAP_COPY}") lines)"
}

# Object file that defines symbol $1 in the link map ("" if none). The map lists each input section
# (.text.<sym>) followed by the object it came from, on the same or the next line.
map_origin() {
    grep -E "^ \.text\.$1([[:space:]]|$)" -A1 "${MAP_COPY}" | grep -oE '[^ /()]+\.(obj|o)' | head -1 || true
}

check_no_heap() {
    local bin="${EXE_DIR}/core-cpu1" syms sym origin defined=() bad=0
    syms="$("${NM}" "${bin}")"
    # Allocator entry points may exist only as the board's traps (syscalls.c); newlib's allocator objects
    # (mallocr, freer, reallocr, callocr, ...) and the FreeRTOS heap must not be linked.
    for sym in malloc free calloc realloc _malloc_r _free_r _calloc_r _realloc_r _sbrk; do
        grep -Eq " [TtWw] ${sym}$" <<<"${syms}" || continue
        origin="$(map_origin "${sym}")"
        if [[ "${origin}" != syscalls.c.obj ]]; then
            log "ERROR ${sym} linked from '${origin:-unknown}', not from the board's syscalls.c trap"
            bad=1
        fi
        defined+=("${sym}")
    done
    if grep -Eq ' [TtWw] (memalign|_memalign_r|posix_memalign|aligned_alloc|_malloc_usable_size_r|pvPortMalloc|vPortFree)$' <<<"${syms}"; then
        log "ERROR other allocator or FreeRTOS heap functions linked"
        bad=1
    fi
    if grep -Eq 'lib_a-(nano-)?(mallocr|freer|callocr|reallocr|malignr|msizer|sbrkr)\.o' "${MAP_COPY}"; then
        log "ERROR newlib allocator objects in the link map:"
        grep -oE 'lib_a-(nano-)?(mallocr|freer|callocr|reallocr|malignr|msizer|sbrkr)\.o' "${MAP_COPY}" | sort -u
        bad=1
    fi
    (( bad == 0 )) || die "dynamic allocation linked into core-cpu1"
    log "OK   no allocator and no heap linked: newlib allocator objects absent from the map;"
    log "     present only as the board traps (syscalls.c): ${defined[*]:-none}"
}

footprint_report() {
    local bin="${EXE_DIR}/core-cpu1" sim
    {
        echo "core-cpu1 (Cortex-M3, board link-check) — sections (arm-none-eabi-size -A):"
        "${SIZE}" -A "${bin}" | grep -vE '^\.(debug|comment|ARM\.attributes)|^$'
        echo
        echo "Totals (Berkeley): $("${SIZE}" "${bin}" | tail -1 | awk '{print "text=" $1 " data=" $2 " bss=" $3}')"
        echo
        echo "20 largest RAM symbols (data and bss), bytes:"
        "${NM}" -S --size-sort -t d "${bin}" | awk '$3 ~ /^[bBdD]$/' | tail -20 | sort -k2,2nr \
            | awk '{ printf "  %10d  %s\n", $2, $4 }'
        echo
        if [[ -x "${SIM_BIN}" ]]; then
            sim="$(size "${SIM_BIN}" | tail -1)"
            echo "Simulator core-cpu1 (x86-64, for comparison): $(awk '{print "text=" $1 " data=" $2 " bss=" $3}' <<<"${sim}")"
        else
            echo "Simulator core-cpu1 not built (run scripts/06 for the comparison)."
        fi
    } >"${REPORT}"
    log "OK   footprint report: ${REPORT}"
    sed 's/^/     /' "${REPORT}" | grep -E 'Totals|Simulator core-cpu1|^\s+[0-9]+\s' | head -8
}

main() {
    parse_args "$@"
    mkdir -p "${LOG_DIR}"
    exec > >(tee -a "${LOG_FILE}") 2>&1
    log "=== 08-build-cfe-arm-cm3 start ==="

    cfe_sim_check_prerequisites
    check_cross_tools
    local before after
    before="$(cfe_sim_sources_status)"

    cfe_sim_build "${DO_CLEAN}"
    check_image
    check_map
    check_no_heap
    cfe_sim_check_static_only
    footprint_report

    after="$(cfe_sim_sources_status)"
    [[ -z "${after}" && "${before}" == "${after}" ]] || die "sources/ working trees are not clean"
    log "OK   sources/cFS and sources/FreeRTOS-Kernel unchanged and clean"
    log "=== 08-build-cfe-arm-cm3 OK ==="
}

main "$@"
