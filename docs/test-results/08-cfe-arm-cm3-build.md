# Test result — 08 cFE cross-build for ARM Cortex-M3

- Date: 2026-11-05
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs), cmake 3.28.3
- Cross toolchain (installed by the user with apt, see `docs/VERSIONS.md`): `arm-none-eabi-gcc` 13.2.1
  (`15:13.2.rel1-2`), binutils 2.42, newlib-nano 4.4.0
- Sources (unmodified in `sources/`): cFS `c62b5b9`, FreeRTOS-Kernel `8a59a4b` (V11.3.2, `GCC/ARM_CM3` port)
- Board: `boards/link-check` — PLACEHOLDER (generic Cortex-M3 start-up, invented memory map and clock). Nothing
  was run: build and link only.
- Decision: D31.
- Command: `./scripts/08-build-cfe-arm-cm3.sh --clean`. Exit 0.

## Results

| Check | Result |
|---|---|
| Configure, build and install with cFE's strict flags (`-Werror`; project code also `-Wextra`); no compiler warnings in the log | PASS |
| `core-cpu1` is ELF32 ARM EABI5, `Tag_CPU_arch: v7`, profile Microcontroller, Thumb-2, no ARM-mode code | PASS |
| Entry point = `Reset_Handler` (`0x00024590`, Thumb) | PASS |
| Vector table: [0] = `_estack`, [1] = `Reset_Handler`, [11] SVCall = `vPortSVCHandler`, [14] PendSV = `xPortPendSVHandler`, [15] SysTick = `xPortSysTickHandler` | PASS |
| Link map written (`logs/08-core-cpu1.map`, 7287 lines) | PASS |
| No newlib allocator object (`mallocr`, `freer`, `callocr`, `reallocr`, `malignr`, `msizer`, `sbrkr`) in the map; `malloc`/`free`/`calloc`/`realloc` and `_malloc_r`/`_free_r`/`_calloc_r`/`_realloc_r` come only from the board's `syscalls.c` traps; `_sbrk` is discarded (not in the image) | PASS |
| No FreeRTOS heap (`pvPortMalloc`/`vPortFree`) and no dynamic FreeRTOS object creation; `CFE_PSP_Main` linked | PASS |
| No target object references an allocator (`nm -u`, 150 objects, `ut-stubs`/`ut_assert` excluded) | PASS — 0 of 150 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |
| Regression in this repository (shared `os-impl-filesys.c` and `scripts/lib/cfe-sim.sh` changed): `06 --clean` 5/5, `05` 5/5, `04 --clean` 75/75, `07-check-bundle` exit 0 (counter_app 5/5) | PASS |

Footprint (`logs/08-footprint.txt`, `arm-none-eabi-size`): text 212077, data 1296, bss 7568804 bytes
(`.isr_vector` 64, `.text` 161524, `.rodata` 50481, `.main_stack` 4096). Largest RAM symbols: PSP reserved
memory 4194304, OSAL RAM pool 1048576, `CFE_TBL_Global` 577488, OSAL queue table 539648, `CFE_SB_Global` 528520,
three OSAL stack pools of 131072. These are the simulator's sizes; the x86-64 simulator image has text 498589,
data 5776, bss 7632720.

## Problems found and fixed on the way

- `os-impl-filesys.c` (shared OS layer): the guard `(uint64)blocksize > UINT32_MAX` is always false when
  `size_t` is 32 bits, and `-Werror=type-limits` stopped the build. Replaced by the width-independent round-trip
  check `(size_t)(uint32)blocksize != blocksize`. Same behaviour on the simulator.
- `--specs=nano.specs` reached the link line twice (CMake adds the C flags to the link) and gcc refused it. It is
  now in the C flags only.
- newlib's allocator was linked: newlib-nano's printf core (used by cFE's `snprintf`/`vsnprintf`) references
  `_malloc_r`/`_free_r`/`_realloc_r`, and stdio allocates the `stdout` buffer on the first `printf`/`puts` (stock
  PSP code: `cfe_psp_module.c`, `soft_timebase`, the eeprom/port/ram "not implemented" modules). Fixed as
  approved: allocator traps in the board's `syscalls.c`, forced with `-u` ahead of the C library (a first try
  without `-u` gave "multiple definition", because libc's objects were already pulled), and `stdout` made
  unbuffered in the BSP `main()` before anything prints.
- Script fixes: the static-memory object scan looked for `*.o` only, but the `Generic` CMake platform names
  objects `*.obj` (first run scanned 1 object); the map-origin parser kept a trailing `)`.

## Not covered

- Nothing ran. That the traps are never hit, that `stdout` really stays unbuffered and that the start-up works
  needs an emulator (step 09) or the board.
- The board files are placeholders; the M2S150 memory map, clock, NVIC priority bits and UART come with the
  Microchip sources (D7). RAM use (7.6 MB bss) is not yet sized for a board.
- Toolchain from Ubuntu's archive, checked on this host only.
