#!/usr/bin/env bash
# scripts/lib/cm3-image.sh — checks of a Cortex-M3 core-cpu1 image, shared by script 08 (cross-build,
# placeholder board) and script 09 (QEMU board). Sourced, not run. The caller sets EXE_DIR, WORK_DIR,
# ARCH_SUBDIR (from cfe_sim_init) and MAP_COPY (where the link map is copied), and provides log and die.
# Source it after cfe_sim_init: it replaces the host binutils names cfe_sim_init sets.

NM="arm-none-eabi-nm"
SIZE="arm-none-eabi-size"
READELF="arm-none-eabi-readelf"
OBJDUMP="arm-none-eabi-objdump"

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

# A global offset table outside .data would never be copied to RAM (every entry 0 at run time).
check_got() {
    local bin="${EXE_DIR}/core-cpu1" hdr
    hdr="$("${OBJDUMP}" -h "${bin}")"
    if grep -Eq '^ +[0-9]+ \.got' <<<"${hdr}"; then
        grep -E '^ +[0-9]+ \.got' <<<"${hdr}"
        die "core-cpu1 has a .got/.got.plt output section outside .data (board.ld must place *(.got*) in .data)"
    fi
    log "OK   no .got/.got.plt outside .data (position-independent objects get their GOT copied with .data)"
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
    # Flight image: allocator entry points may exist only as the traps (boards/common-cm3/alloc_traps.c, never
    # the test allocator test_heap.c) and _sbrk only as the syscalls.c trap; newlib's allocator objects
    # (mallocr, freer, reallocr, callocr, ...) and the FreeRTOS heap must not be linked.
    local want
    for sym in malloc free calloc realloc _malloc_r _free_r _calloc_r _realloc_r _sbrk; do
        grep -Eq " [TtWw] ${sym}$" <<<"${syms}" || continue
        origin="$(map_origin "${sym}")"
        want="alloc_traps.c.obj"
        [[ "${sym}" == _sbrk ]] && want="syscalls.c.obj"
        if [[ "${origin}" != "${want}" ]]; then
            log "ERROR ${sym} linked from '${origin:-unknown}', not from the trap in ${want%.obj}"
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
    log "     present only as the traps (alloc_traps.c, _sbrk in syscalls.c): ${defined[*]:-none}"
}

# RAM budget of the flight image (step 10, D36): .data + .bss + .main_stack must not exceed RAM_BUDGET_BYTES
# (default 524288 = 512 KiB, provisional until the SmartFusion2 memory map is known, D7). Flight images only
# (scripts 08 and 09); the test images (09b, 09c) keep larger sizes.
check_ram_budget() {
    local bin="${EXE_DIR}/core-cpu1" budget="${RAM_BUDGET_BYTES:-524288}" used
    [[ "${budget}" =~ ^[1-9][0-9]*$ ]] || die "RAM_BUDGET_BYTES must be a positive integer (got '${budget}')"
    # readelf -SW: "[ N] name type addr off size ..." (the index may split into two fields).
    used="$("${READELF}" -SW "${bin}" | awk '
        { for (i = 1; i < NF; i++) if ($i == ".data" || $i == ".bss" || $i == ".main_stack") {
              x = $(i + 4); v = 0
              for (j = 1; j <= length(x); j++) v = v * 16 + index("0123456789abcdef", substr(x, j, 1)) - 1
              s += v; n++ } }
        END { if (n == 3) print s }')"
    [[ -n "${used}" ]] || die "core-cpu1: .data, .bss or .main_stack section not found"
    if (( used > budget )); then
        die "RAM budget exceeded: .data + .bss + .main_stack = ${used} bytes > ${budget} (RAM_BUDGET_BYTES, D36)"
    fi
    log "OK   RAM budget: .data + .bss + .main_stack = ${used} bytes <= ${budget} ($(( used * 100 / budget )) %)"
}
