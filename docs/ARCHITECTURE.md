# Architecture — cFE and OSAL on FreeRTOS

How the pinned NASA cFE and OSAL run on FreeRTOS V11.3.2 in this project. Current scope: steps 04a (OSAL core
layer and the Linux simulator BSP), 04b (static RAM file system), 04c (all OSAL functional tests) and 05 (PSP and
cFE core on the simulator), 05b (cFE's functional tests), 06 (`counter_app`), 07 (shareable bundle), 08 (Cortex-M3 cross-build on a placeholder board), 09 (Cortex-M3 run on QEMU
`mps2-an385`), 09b (OSAL functional tests on QEMU) and 09c (cFE functional tests on QEMU). The SmartFusion2 board files do not exist yet (D7). Decisions are in `DECISIONS.md`.

## Layers

```
 counter_app / OSAL tests           apps/counter_app (static, D28)      project code
 ───────────────────────────────────────────────────────────────
 cFE core (ES, EVS, SB, TBL, TIME)  sources/cFS/cfe                     pinned, unmodified
 ───────────────────────────────────────────────────────────────
 cFE PSP "freertos-posix-sim"      psp-ext/ + stock PSP shared code    project code (D27)
   start-up, static reserved memory, /cf + embedded script, timebase_freertos
 ───────────────────────────────────────────────────────────────
 OSAL public API + shared layer     sources/cFS/osal/src/os/shared      pinned, unmodified
   argument checks, object id tables, OS_TimeBase_CallbackThread, console ring buffer
 ───────────────────────────────────────────────────────────────
 OSAL OS layer "freertos"           osal-ext/freertos/                  project code (D8, D17)
   *_Impl functions on FreeRTOS static APIs
   + OSAL portable "no-*" blocks for unsupported features
 ───────────────────────────────────────────────────────────────
 OSAL BSP "freertos-posix-sim"      osal-ext/freertos-posix-sim/        project code
   main(), start-up task, console output, BSP lock
 ───────────────────────────────────────────────────────────────
 FreeRTOS V11.3.2 kernel            sources/FreeRTOS-Kernel             pinned, unmodified
   + POSIX port, patched copy (patch 0001, D14) + idle hook (D15)
 ───────────────────────────────────────────────────────────────
 Linux host (pthreads, signals)     simulator only
```

The Cortex-M3 build (step 08) keeps the top layers, replaces the BSP and PSP platform with `freertos-arm-cm3`, and
uses the `GCC/ARM_CM3` port instead of the POSIX port; see "Cortex-M3 build" below.

## How OSAL finds the project code

OSAL's CMake is run directly on `sources/cFS/osal` with:

| Setting | Value |
|---|---|
| `OSAL_EXT_SOURCE_DIR` | `osal-ext/`, so OSAL looks for `osal-ext/<ostype>` and `osal-ext/<bsptype>` |
| `OSAL_SYSTEM_OSTYPE` | `freertos` |
| `OSAL_SYSTEM_BSPTYPE` | `freertos-posix-sim` |
| `OSAL_CONFIGURATION_FILE` | `config/osal/freertos-posix-sim/osal_config.cmake` (network and dynamic loader off, static loader on, permissive mode off) |

The BSP's CMake builds the kernel through `cmake/freertos-posix-kernel.cmake` (shared with the step 03 smoke
test), which applies patch 0001 to a copy of `port.c` in the build tree. The kernel is attached to
`osal_public_api`, so everything that links OSAL also links FreeRTOS. Script: `scripts/04-build-osal-freertos-sim.sh`.

## OS layer (`osal-ext/freertos/`)

| Area | File | Design |
|---|---|---|
| Tasks | `os-impl-tasks.c` | Static TCB slots (`OS_MAX_TASKS + 8`) and stack pools in size classes, 4 KiB×16, 8 KiB×16, 16 KiB×8, 32 KiB×4 (D18). A task takes the smallest stack that fits and gives it back on delete. A task that exits itself becomes a suspended "zombie", reclaimed by the next task create. The OSAL id is kept in TLS pointer 0. |
| Priorities | `os-impl-common.c` | OSAL 0 (highest) … 255 → FreeRTOS 30 … 1. FreeRTOS 0 is the idle task, 31 the timer task. 32 priorities in `FreeRTOSConfig.h`. |
| Semaphores | `os-impl-sem.c`, `-binsem.c`, `-countsem.c` | Own semaphore core, because FreeRTOS has no flush. Each waiter is a record on its own stack in a priority-ordered list; it sleeps on task notification index 1. Give hands the count straight to the first waiter; flush and delete wake all. The list is only touched with the scheduler suspended. TLS pointer 1 lets task delete remove a waiting task from the list. |
| Mutexes | `os-impl-mutex.c` | FreeRTOS static recursive mutexes, which give priority inheritance. |
| Queues | `os-impl-queues.c` | One static FreeRTOS queue per OSAL queue. Items are `[length][payload]`, to support variable-size OSAL messages. Put and get staging buffers are each protected by a static mutex. Storage is 8 KiB per queue; a queue that does not fit is rejected with `OS_QUEUE_INVALID_SIZE`. |
| Object tables | `os-impl-idmap.c` | One static mutex per OSAL object type protects that type's table. |
| Time bases | `os-impl-timebase.c` | One static helper task per time base runs OSAL's callback thread, sleeping to absolute tick deadlines. Delete is cooperative, with a forced delete as fallback. The FreeRTOS timer service is not used. |
| Clock | `os-impl-clock.c` | Local time = time set + ticks since (32-bit tick extended to 64 bits). 1 kHz tick. |
| Console | `os-impl-console.c` | `OS_printf` → OSAL ring buffer → static console task → BSP output. |
| Files | `os-ramfs.c`, `os-impl-filesys.c` | Static RAM file system (D19, D25). The core keeps a volume in one memory area: header, file/directory entries (one per 2 KiB of volume, at least `OS_MAX_NUM_OPEN_FILES` + 8), a block table linking each file's fixed-size blocks, and the blocks. `OS_mkfs` uses the caller's memory, or an area of a static pool if none is given (1 MiB on the simulator, D26). RAM disk paths are `ramfs:<n>/…`; fixed maps such as `/cf` are directories of a built-in 32 KiB root volume. One static mutex serialises all file calls. |
| Not supported | OSAL `portable/os-impl-no-*.c` | Condition variables, shell, loader, symbol table, network, sockets, select. |

All FreeRTOS objects are created with the `*Static` APIs from memory defined in this layer.
`FreeRTOSConfig.h` sets `configSUPPORT_DYNAMIC_ALLOCATION 0`, and script 04 also fails if any dynamic FreeRTOS
creation symbol or the FreeRTOS heap appears in a test binary. Evidence:
`test-results/04a-osal-freertos-sim.md`.

## Simulator BSP (`osal-ext/freertos-posix-sim/`)

- `main()` pins the process to one host CPU (D21), creates the static `BSP_MAIN` task (FreeRTOS priority 29)
  and starts the scheduler. `BSP_MAIN` runs `OS_Application_Startup()` and `OS_Application_Run()`, then calls
  `exit()` with the application status.
- Console output uses `write(1)`, not stdio, to avoid glibc stdio locks across port task switches.
- `OS_BSP_Lock_Impl` is a static recursive FreeRTOS mutex. `configASSERT` failures print and `abort()`.

## PSP (`psp-ext/`) and cFE build (step 05)

cFE only finds a PSP platform inside the PSP tree (`psp/fsw/<CFE_SYSTEM_PSPNAME>`). Script 05 therefore copies
`sources/cFS/psp` to `build/cfe-freertos-sim/psp-tree/psp`, adds `psp-ext/fsw/freertos-posix-sim` and
`psp-ext/fsw/modules/timebase_freertos`, and passes the copy to cFE with `CFS_APP_PATH` (D27). The mission config is
`config/cfe/freertos-posix-sim/`: one CPU, toolchain `freertos-posix-sim` (OSTYPE `freertos`, BSP and PSP
`freertos-posix-sim`), the step 04 OSAL config, cFE's strict compile flags. The scripts assemble the actual
`MISSION_DEFS` in the build directory (`scripts/lib/cfe-sim.sh`): the base config, an optional mission overlay
(`config/cfe/freertos-posix-sim-ft/` for the functional tests), `m2s_paths.cmake` and the `/cf` image `cf.tar`.

Linking (D29): executables link their libraries in a GNU ld group, because cFE links its core modules with
`--whole-archive` only when a CPU has dynamically loaded apps; and with `-u CFE_PSP_Main`, so the PSP's
`OS_Application_Startup` is used and not OSAL's fallback in `libosal_bsp`.

`/cf` image (D5, D29): the scripts pack `cfe_es_startup.scr` and the files listed in the mission's `cf_files.txt`
(e.g. table images from `elf2cfetbl`) into a deterministic POSIX ustar archive, embedded with cFE's
`cpu1_EMBED_FILELIST "CFE_PSP_CF_IMAGE,cf.tar"`. Missions whose tables are generated by the build are built twice
(first pass makes the tables, second embeds them).

| Area | File | Design |
|---|---|---|
| Start-up | `cfe_psp_start.c` | `OS_Application_Startup` (called by the BSP task): OSAL init → reserved memory map → `/cf` fixed map → embedded `/cf` image unpacked (regular files and directories; absolute paths, `..` and over-long names refused) → PSP modules → `CFE_ES_Main`, always a power-on reset. `OS_Application_Run` sleeps forever. |
| Reserved memory | `cfe_psp_memory.c` | One static 4 MiB array, laid out at start-up from the sizes cFE reports (reset area, 2 MiB `/ram` disk, CDS, user area); start-up stops if they do not fit. CDS reads/writes are bounds-checked. |
| Time base | `modules/timebase_freertos` | FreeRTOS tick extended to 64 bits; 1 ms resolution. Shared by simulator and board. |
| Restart, panic | `cfe_psp_support.c` | End the simulator process. |
| Watchdog, exceptions, SSR | `cfe_psp_watchdog.c`, `cfe_psp_exception.c`, `cfe_psp_ssr.c` | Stubs (none on the simulator). |
| Stock modules | `soft_timebase`, `eeprom_notimpl`, `ram_direct`, `port_notimpl`, `iodriver` | Unchanged. |

## Applications (step 06)

Apps are linked into `core-cpu1` (the dynamic loader is off): `cpu1_STATIC_APPLIST counter_app`, and
`cpu1_STATIC_SYMLIST "COUNTER_APP_Main,COUNTER_APP"` makes cFE generate OSAL's static symbol table. ES reads the
startup script line `CFE_APP, counter_app, COUNTER_APP_Main, COUNTER_APP, 50, 16384, 0x0, 0;`, calls
`OS_ModuleLoad` with the cFE name `COUNTER_APP` (found in the static table) and looks up the entry point there.
`counter_app` uses only cFE/OSAL APIs and prints with `OS_printf`, which goes OSAL console → BSP → terminal (UART on
the board). Its 1 s period follows fixed deadlines from `OS_GetLocalTime` (D28). Scripts 05, 05b and 06 share the
build steps in `scripts/lib/cfe-sim.sh`; `apps/` is on cFE's module search path (`CFS_APP_PATH`).

## Functional tests (step 05b)

Mission `freertos-posix-sim-ft` links cFE's `cfe_assert` and `cfe_testcase`, NASA's `sample_lib` and `sample_app`
(the tests share its table) and the test-only `apps/ft_logdump` statically; its `/cf` root volume is 1 MiB for the
test log. After the tests, `ft_logdump` prints `/cf/cfe_test.log` as `FTLOG|`/`FTLOG+` lines; script 05b rebuilds the
file, checks its size, and compares every assertion with stock cFE run natively with the same apps (D29).

## Shareable bundle (steps 07 and 07b)

`scripts/07-export-bundle.sh` generates `cfs-m2s-freertos` (D30, D35), a copy in the stock cFS bundle layout
with two mission configurations:

| Here | In the bundle |
|---|---|
| `osal-ext/freertos` | `osal/src/os/freertos` |
| `osal-ext/freertos-posix-sim` + `cmake/freertos-posix-kernel.cmake`, `config/freertos/posix/*`, patch 0001 | `osal/src/bsp/freertos-posix-sim` (+ `cmake/`, `config/`, `patches/`) |
| `osal-ext/freertos-arm-cm3` + `cmake/freertos-arm-cm3-kernel.cmake`, `cmake/arm-cm3-flags.cmake`, `config/freertos/arm_cm3/*`, `boards/common-cm3`, `boards/qemu-mps2-an385` | `osal/src/bsp/freertos-arm-cm3` (+ `cmake/`, `config/`, `boards/`) |
| `psp-ext/fsw/freertos-posix-sim`, `psp-ext/fsw/freertos-arm-cm3`, `psp-ext/fsw/modules/timebase_freertos` | `psp/fsw/...` |
| `apps/counter_app` | `apps/counter_app` |
| `config/cfe/freertos-posix-sim`, `config/cfe/freertos-arm-cm3(-qemu)` (via the scripts) | `freertos_sim_defs/`, `freertos_qemu_defs/` (stock `sample_defs` + `export/<name>_defs`; `<name>_perfids.h` = stock `sample_perfids.h`; `cf.tar` built by `mission_build_custom.cmake`) |
| `sources/cFS`, `sources/FreeRTOS-Kernel` | top level, `FreeRTOS-Kernel/` |

Users pick a configuration with `make MISSIONCONFIG=<name> O=<dir> prep`. The kernel helpers take their kernel
and config locations from `M2S_FREERTOS_*` variables; the BSPs set them in the bundle, and this repository uses
the defaults. The export is committed on top of the published bundle's `main` (a clone of it in
`build/export/cfs-m2s-freertos`), with a team-facing message and version tags, so the team can `git pull`;
pushing is separate and manual. `export/<name>_defs` must be kept in step with `config/cfe/...` by hand; script
`07-check-bundle.sh` builds and runs both configurations from a fresh clone to catch drift.

## Cortex-M3 build and QEMU run (steps 08 and 09, D31, D32)

```
 counter_app, cFE, OSAL shared + OS layer "freertos"   same sources as the simulator
 cFE PSP "freertos-arm-cm3"     psp-ext/fsw/freertos-arm-cm3: simulator PSP sources + cfe_psp_support.c
                                (reset through SCB AIRCR, panic prints "M2S PANIC" and halts)
 OSAL BSP "freertos-arm-cm3"    osal-ext/freertos-arm-cm3: main(), unbuffered stdout, BSP_MAIN static task,
                                console -> M2S_Board_ConsoleWrite, assert / stack-overflow hooks print and halt
 generic Cortex-M3 code         boards/common-cm3 (M2S_CM3_COMMON_DIR): startup_cm3.c (vector table, fault
                                report, Reset_Handler), syscalls.c (newlib stubs, _sbrk trap), alloc_traps.c
                                (flight) or test_heap.c (test images only, D33), m2s_cm3_diag.c ("M2S ..." lines,
                                M2S_Program_Exit)
 board folder (M2S_BOARD_DIR)   m2s_board.h (clock, NVIC priority bits), board.c (console, M2S_Board_Exit),
                                board.ld
 FreeRTOS GCC/ARM_CM3 port      unmodified; config/freertos/arm_cm3/FreeRTOSConfig.h; no FreeRTOS heap
```

- Mission `config/cfe/freertos-arm-cm3` (toolchain `arm-none-eabi-gcc`, newlib-nano, `Generic` platform). Script
  `08-build-cfe-arm-cm3.sh` builds it with the same script library as the simulator (`scripts/lib/cfe-sim.sh`,
  mission base and binutils as parameters).
- Boards (`M2S_BOARD_NAME`, default `link-check`; a mission overlay's `m2s_board.cmake` can name another):
  - `boards/link-check` (step 08): PLACEHOLDER — invented memory map (16 MiB code at 0, 64 MiB RAM at
    `0x20000000`), placeholder clock and NVIC priority bits, console that discards output;
  - `boards/qemu-mps2-an385` (step 09, mission overlay `config/cfe/freertos-arm-cm3-qemu`): QEMU's ARM MPS2
    AN385 — code in SSRAM1 at 0, data/bss/main stack in PSRAM at `0x21000000`, 25 MHz, 3 priority bits,
    console on the CMSDK UART0 at `0x40004000` (polled);
  - the SmartFusion2 board folder comes with the board sources (D7).
- `Reset_Handler` first quiesces what a boot loader may have left running (SysTick off, NVIC enables and
  pending bits cleared, PendSV/SysTick pending cleared), sets VTOR to the vector table, prepares `.data`/`.bss`,
  enables interrupts and calls `main()`. Faults print one "M2S FAULT" line (exception number, CFSR, HFSR,
  MMFAR, BFAR, stacked PC/LR/xPSR) and halt; traps, asserts, panic and BSP stop print "M2S TRAP/ASSERT/PANIC/
  STOP" lines. Scripts treat any of them as a failure.
- Program end (D33): `exit()`/`_exit`, the end of `OS_Application_Run` (test programs) and BSP stops go
  through `M2S_Program_Exit(status)` → `M2S_Board_Exit`: on QEMU an ARM semihosting exit, so QEMU ends with the
  status; on other boards "M2S STOP" and halt.
- OSAL test images (step 09b): OSAL built on its own with `config/osal/freertos-arm-cm3/toolchain.cmake`
  (shared settings in `cmake/arm-cm3-flags.cmake`, board `qemu-mps2-an385`, libraries in one ld group, one
  map per program). They link the test-only allocator because `ut_assert` uses `malloc`; flight images never.
- Linker scripts place `.got`/`.got.plt` inside `.data` (D34): position-independent objects (OSAL's `ut_assert`)
  reach globals through the GOT, which must be copied to RAM with the other initialised data.
- cFE functional tests on the Cortex-M3 (step 09c): mission `freertos-arm-cm3-qemu-ft` (copy of the simulator's
  functional-test mission, test-only allocator), comparison code shared with 05b in `scripts/lib/cfe-ft.sh`;
  QEMU default timing there (D34).
- QEMU runs with `-icount shift=auto,sleep=on,align=on` in steps 09 and 09b (D32): guest time from the instruction count, held to
  host real time. In QEMU's default mode SysTick follows the host clock and late ticks are never made up.
- No heap at link time: `boards/common-cm3/syscalls.c` defines `malloc`/`free`/`calloc`/`realloc` and their newlib `_r` forms as traps;
  the toolchain's `-u` options make the linker take them from the kernel/board library before the C library, so
  newlib-nano's allocator (referenced by its printf core and stdio) is never linked and `_sbrk` is discarded.
- Memory sizes are still the simulator's (bss about 7.6 MB, mostly PSP reserved memory 4 MiB, OSAL RAM pool
  1 MiB, cFE table and SB globals, OSAL queue table); they must be cut to the board's RAM later.

## Host-side versus embedded memory

On the simulator, the FreeRTOS POSIX port allocates its per-thread bookkeeping with glibc `malloc` and runs each
task on a host pthread stack; OSAL's test harness (`ut_assert`) also uses `malloc`. These are host-only. The OS
layer, the BSP, OSAL's shared layer, the PSP and cFE do not reference `malloc`/`free` (step 05 checks every object
linked into `core-cpu1`). The Cortex-M3 image has its own evidence (step 08): no target object references an
allocator, newlib's allocator objects are absent from the link map, and the allocator entry points exist only as
the board's traps.

## Simulator limitations

- A task does not run on the stack OSAL gives it (POSIX port), so osal-core-test 02.003 fails by design (D22).
- Features off by configuration (network, loading modules from files) make 12 OSAL assertions fail; script 04
  tolerates exactly those (D26).
- Timing depends on the host: the old VirtualBox VM could starve threads for seconds (D14, D15, D21, D24).
