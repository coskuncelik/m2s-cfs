# Architecture — OSAL on FreeRTOS

How the pinned NASA OSAL runs on FreeRTOS V11.3.2 in this project. Current scope: steps 04a (OSAL core layer and
the Linux simulator BSP) and 04b (static RAM file system). cFE, the PSP and the SmartFusion2 BSP do not exist yet. Decisions are in `DECISIONS.md`.

## Layers

```
 application / OSAL tests           (later: cFE + counter_app)
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

The planned SmartFusion2 target keeps the top three layers, replaces the BSP with a board BSP, and uses the
`GCC/ARM_CM3` port instead of the POSIX port.

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
| Files | `os-ramfs.c`, `os-impl-filesys.c` | Static RAM file system (D19, D25). The core keeps a volume in one memory area: header, file/directory entries (one per 2 KiB of volume), a block table linking each file's fixed-size blocks, and the blocks. `OS_mkfs` uses the caller's memory, or an area of a 256 KiB static pool if none is given. RAM disk paths are `ramfs:<n>/…`; fixed maps such as `/cf` are directories of a built-in 32 KiB root volume. One static mutex serialises all file calls. |
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

## Host-side versus embedded memory

On the simulator, the FreeRTOS POSIX port allocates its per-thread bookkeeping with glibc `malloc` and runs each
task on a host pthread stack; OSAL's test harness (`ut_assert`) also uses `malloc`. These are host-only. The OS
layer, the BSP and OSAL's shared layer do not reference `malloc`/`free`. The Cortex-M3 image will need its own
evidence (link map, `malloc`/`_sbrk` trap), which does not exist yet.

## Simulator limitations

- A task does not run on the stack OSAL gives it (POSIX port), so osal-core-test 02.003 fails by design (D22).
- Timing depends on the host: the old VirtualBox VM could starve threads for seconds (D14, D15, D21, D24).
