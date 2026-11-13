# Porting notes — cFE and OSAL on FreeRTOS

This file explains how the pinned cFE, OSAL and PSP run on FreeRTOS V11.3.2 in this bundle. The details
are in the file headers. The generated sections at the end list the pinned versions and every file this
bundle adds to or changes in the upstream trees.

## Layers

```
 counter_app                        apps/counter_app                      added
 ─────────────────────────────────────────────────────────────────
 cFE core (ES, EVS, SB, TBL, TIME)  cfe/                                  upstream, unmodified
 ─────────────────────────────────────────────────────────────────
 cFE PSP "freertos-posix-sim"       psp/fsw/freertos-posix-sim            added
   start-up, static reserved memory, /cf image, timebase_freertos module
 ─────────────────────────────────────────────────────────────────
 OSAL public API + shared layer     osal/src/os/shared                    upstream, unmodified
 ─────────────────────────────────────────────────────────────────
 OSAL OS layer "freertos"           osal/src/os/freertos                  added
   *_Impl functions on FreeRTOS static APIs + OSAL portable "no-*" blocks
 ─────────────────────────────────────────────────────────────────
 OSAL BSP "freertos-posix-sim"      osal/src/bsp/freertos-posix-sim       added
   main(), start-up task, console output, BSP lock, kernel build
 ─────────────────────────────────────────────────────────────────
 FreeRTOS V11.3.2 kernel            FreeRTOS-Kernel/                      upstream, unmodified
   POSIX port: patched copy of port.c in the build directory, idle hook
 ─────────────────────────────────────────────────────────────────
 Linux host (pthreads, signals)     simulator only
```

The Cortex-M3 target (configuration `freertos_qemu`) keeps the top layers and replaces the bottom ones:

```
 cFE PSP "freertos-arm-cm3"         psp/fsw/freertos-arm-cm3              added
   the simulator PSP sources + Cortex-M3 restart (SCB AIRCR) and panic
 ─────────────────────────────────────────────────────────────────
 OSAL BSP "freertos-arm-cm3"        osal/src/bsp/freertos-arm-cm3         added
   main(), unbuffered stdout, start-up task, console -> board, assert/stack-overflow hooks, kernel build
 ─────────────────────────────────────────────────────────────────
 generic Cortex-M3 code             .../freertos-arm-cm3/boards/common-cm3   added
   vector table, Reset_Handler, fault report, newlib stubs, allocator traps, "M2S ..." diagnostics
 board "qemu-mps2-an385"            .../freertos-arm-cm3/boards/qemu-mps2-an385   added
   m2s_board.h (clock, NVIC priority bits), board.c (UART console, exit), board.ld (memory map)
 ─────────────────────────────────────────────────────────────────
 FreeRTOS V11.3.2 kernel            FreeRTOS-Kernel/                      upstream, unmodified
   GCC/ARM_CM3 port, no FreeRTOS heap
```

## How the build finds the port

- Each configuration's toolchain file sets `OSAL_SYSTEM_OSTYPE=freertos` and the BSP and PSP names:
  `freertos_sim_defs/toolchain-freertos-posix-sim.cmake` → `freertos-posix-sim`,
  `freertos_qemu_defs/toolchain-freertos-arm-cm3.cmake` → `freertos-arm-cm3`.
- OSAL then builds `osal/src/os/freertos` and `osal/src/bsp/<bsp>`, and cFE builds `psp/fsw/<psp>`. Both are
  found by the stock CMake rules.
- The BSP's `CMakeLists.txt` builds the kernel from `FreeRTOS-Kernel/`, with these parts:
  - `config/FreeRTOSConfig.h`;
  - the idle hook;
  - a copy of the POSIX `port.c`, patched in the build directory with
    `patches/0001-posix-port-tick-catch-up.patch`.

  The kernel is attached to `osal_public_api`, so everything that links OSAL also links FreeRTOS.
- To use another kernel tree, set the CMake cache variable `M2S_FREERTOS_DIR`.
- Cortex-M3: the toolchain file also names the board folder (`M2S_BOARD_DIR`, default
  `boards/qemu-mps2-an385`) and the generic code (`M2S_CM3_COMMON_DIR`), and takes the compiler and linker
  settings from `osal/src/bsp/freertos-arm-cm3/cmake/arm-cm3-flags.cmake`: `arm-none-eabi-gcc`,
  `-mcpu=cortex-m3 -mthumb -mfloat-abi=soft`, newlib-nano, `-nostartfiles`, the board's linker script,
  `--gc-sections`, a link map, and `-u` options that make the linker take the allocator entry points from the
  traps before the C library. The BSP builds the kernel with the unmodified `GCC/ARM_CM3` port and
  `config/FreeRTOSConfig.h`, which reads the clock and priority bits from the board's `m2s_board.h`.

## OS layer (`osal/src/os/freertos`)

| Area | File | Design |
|---|---|---|
| Tasks | `os-impl-tasks.c` | Static TCB slots and stack pools in size classes (4 KiB×16, 8 KiB×16, 16 KiB×8, 32 KiB×4). A task takes the smallest stack that fits and gives it back on delete. A task that exits itself becomes a suspended "zombie", and the next task create reclaims it. The OSAL id is kept in TLS pointer 0. |
| Priorities | `os-impl-common.c` | OSAL 0 (highest) … 255 maps to FreeRTOS 30 … 1. FreeRTOS 0 is the idle task, 31 the timer task. |
| Semaphores | `os-impl-sem.c`, `-binsem.c`, `-countsem.c` | Own semaphore core, because FreeRTOS has no flush. Waiters wait in a priority-ordered list and sleep on task notification index 1. Give hands the count straight to the first waiter; flush and delete wake all waiters. |
| Mutexes | `os-impl-mutex.c` | FreeRTOS static recursive mutexes, which give priority inheritance. |
| Queues | `os-impl-queues.c` | One static FreeRTOS queue per OSAL queue. Items are `[length][payload]`, to support variable-size messages. Storage is 8 KiB per queue. |
| Time bases | `os-impl-timebase.c` | One static helper task per time base, sleeping to absolute tick deadlines. The FreeRTOS timer service is not used. |
| Clock | `os-impl-clock.c` | Local time = the time set + ticks since then (the 32-bit tick extended to 64 bits). The tick is 1 kHz. |
| Console | `os-impl-console.c` | `OS_printf` → OSAL ring buffer → static console task → BSP output. |
| Files | `os-ramfs.c`, `os-impl-filesys.c` | Static RAM file system. A volume is one memory area: entries, a block table and fixed-size blocks. `OS_mkfs` uses the caller's memory, or a static pool (`OS_FREERTOS_RAMFS_POOL_BYTES`, 1 MiB). Fixed maps such as `/cf` are directories of a built-in root volume (`OS_FREERTOS_RAMFS_ROOT_BYTES`). |
| Sockets (simulator only) | `os-impl-host-sockets.c` + OSAL `portable/os-impl-bsd-sockets.c`, `os-impl-posix-network.c` | Built when OSAL networking is on and the BSP is `freertos-posix-sim`. Host sockets, non-blocking; every wait polls with a zero-timeout `select()` and blocks the task for one tick between polls (a FreeRTOS POSIX-port task in a blocking host call starves lower-priority tasks). Socket streams share the stream table with RAM files and are dispatched in `OS_Generic*_Impl`. Other BSPs keep the stubs below; networking on with another BSP stops the build. |
| Not supported | OSAL `portable/os-impl-no-*.c` | Condition variables, shell, loader, symbol table; network, sockets and select on the Cortex-M3. |

All FreeRTOS objects are created with the `*Static` APIs from memory defined in this layer.

## PSP (`psp/fsw/freertos-posix-sim`, `psp/fsw/freertos-arm-cm3`)

The Cortex-M3 PSP compiles the simulator PSP's portable sources (start-up and `/cf` image, reserved memory,
watchdog, exceptions, SSR) with its own `cfe_psp_config.h`, plus its own `cfe_psp_support.c`.

| Area | File | Design |
|---|---|---|
| Start-up | `cfe_psp_start.c` | Order: OSAL init → reserved memory map → `/cf` fixed map → embedded `/cf` image unpacked → PSP modules → `CFE_ES_Main`. The image is a POSIX ustar archive; regular files and directories are unpacked, and absolute paths, `..` and over-long names are refused. Every start is a power-on reset. |
| Reserved memory | `cfe_psp_memory.c` | One static array (`CFE_PSP_FREERTOS_RESERVED_BYTES`, 4 MiB), laid out at start-up from the sizes cFE reports. Start-up stops if they do not fit. |
| Time base | `psp/fsw/modules/timebase_freertos` | The FreeRTOS tick, extended to 64 bits; 1 ms resolution. |
| Restart, panic | `cfe_psp_support.c` | Simulator: end the process. Cortex-M3 (`psp/fsw/freertos-arm-cm3`): restart through the SCB AIRCR (`SYSRESETREQ`); panic prints `M2S PANIC` and stops. |
| Watchdog, exceptions, SSR | `cfe_psp_watchdog.c`, `cfe_psp_exception.c`, `cfe_psp_ssr.c` | Stubs. |

## Cortex-M3 board code (`osal/src/bsp/freertos-arm-cm3/boards`)

| Part | File | Design |
|---|---|---|
| Start-up | `common-cm3/startup_cm3.c` | Vector table (16 core entries; SVCall, PendSV and SysTick go straight to the FreeRTOS port). `Reset_Handler` first quiesces what a boot loader may have left running (SysTick off, NVIC enables and pending bits cleared), sets VTOR, copies `.data` (with the GOT of position-independent objects), clears `.bss`, enables interrupts and calls `main()`. |
| Faults | `common-cm3/startup_cm3.c` | One `M2S FAULT` line (exception number, CFSR, HFSR, MMFAR, BFAR, stacked PC/LR/xPSR), then halt. |
| C library | `common-cm3/syscalls.c` | newlib stubs; `_write` goes to the board console; `_sbrk` traps. |
| No heap | `common-cm3/alloc_traps.c` | `malloc`/`free`/`calloc`/`realloc` and the `_r` forms trap. `common-cm3/test_heap.c` is a static test-only allocator for test programs (CMake option `M2S_CM3_TEST_HEAP`, off in these configurations). |
| Board | `qemu-mps2-an385/` | 25 MHz, 3 NVIC priority bits; code in SSRAM1 at 0, data/bss/main stack in PSRAM at `0x21000000`; console on the CMSDK UART0 (polled); exit through semihosting when QEMU allows it. |

A new board is a new folder next to `qemu-mps2-an385` with the same three files (`m2s_board.h`, `board.c`,
`board.ld`), selected with `M2S_BOARD_DIR` in the toolchain file.

## Mission configurations (`freertos_sim_defs`, `freertos_qemu_defs`)

- **Apps are linked statically.** `cpu1_STATIC_APPLIST` (`sample_lib counter_app`; on the simulator also
  `sch_lab ci_lab to_lab`), and `cpu1_STATIC_SYMLIST` fills OSAL's static symbol table. ES loads each app or
  library by its cFE name from the startup script.
- **The `/cf` image.** `mission_build_custom.cmake` packs `cpu1_cfe_es_startup.scr` into `cf.tar` at configure
  time (cFE's embed rule needs the file), and again at build time together with every table image the build
  made for cpu1 (target `m2s-cf-image`, after cFE's table tool and before `core-cpu1` is built), so a single
  `make` puts the default tables of `sch_lab` and `to_lab` into `/cf`. `cpu1_EMBED_FILELIST` embeds `cf.tar` in
  `core-cpu1`; an unchanged archive is byte-identical and causes no relink.
- **Table sources.** cFE uses `<config>_defs/tables/<table>.c` in place of an app's default table source:
  the simulator configuration replaces `sch_lab`'s schedule (counter_app, ci_lab and to_lab housekeeping
  requests, ci_lab's READ_UPLINK at 10 Hz) and `to_lab`'s subscriptions (counter_app housekeeping, ci_lab/to_lab
  housekeeping, events). `arch_build_custom.cmake` puts counter_app's public headers on their include path.
- **Networking** is on only in the simulator configuration (`freertos-posix-sim_osconfig.cmake`).
- **Linking** (`arch_build_custom.cmake`):
  - libraries are linked in a GNU ld group, because the core modules are only linked `--whole-archive`
    when a CPU has dynamic apps;
  - `-u CFE_PSP_Main` makes the linker use the PSP's start-up, not OSAL's fallback
    `OS_Application_Startup`.
- **To add an app:**
  - put it under `apps/`;
  - add it to `cpu1_STATIC_APPLIST` and `cpu1_STATIC_SYMLIST`;
  - add a startup script line whose 4th field equals the symbol list name.

## Validation

The port was tested on both targets before export. On the simulator:

- the OSAL functional and unit test programs (except the assertions for features this layer leaves out:
  loading modules from files);
- cFE start-up;
- `counter_app` timing (1 s ± 50 ms per interval, under 10 ms drift over 20 s), its start-up file test, library
  call and table load;
- OSAL's network and select tests with host sockets;
- commands and telemetry over UDP: every counter_app command and its error cases (wrong length, unknown code,
  period outside the table's limits) through `ci_lab`, with the effects checked in the housekeeping and event
  telemetry from `to_lab`; the ground station's command bytes equal cFE's `cmdUtil` for the same commands;
- cFE's own functional tests (`cfe_testcase` with `cfe_assert`), with all 2521 assertions compared one by one
  against stock cFE on native Linux.

The only allowed difference is one assertion in `es_task_test.c`. It checks that a task's local variable
lies inside the stack buffer given to it, and on the POSIX port it does not (see Limitations in `README.md`).

On the Cortex-M3 under QEMU:

- the image checks: ARMv7-M Thumb-2, entry point and vector table, no allocator or heap linked;
- `counter_app` timing, with the same limits as on the simulator;
- the same OSAL functional and unit test programs, with the same results, except that the stack-address
  check passes here (a task runs on its own stack);
- cFE's own functional tests: all 2521 assertions equal to stock cFE on native Linux, with no allowed
  difference.

These test setups are not part of this bundle.
