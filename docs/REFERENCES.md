# Reference repositories

Reference material only. Code is **not** copied from these repositories (D8). Findings come from the
initial analysis done on 2026-11-03.

## Repositories and revisions

| Repository | Revision | Role |
|---|---|---|
| https://github.com/projetoChi2p/fs-nasa-cfs-mission-v0 | `ae4fe23e68735b8717ac0afeb0355fd863cd6ded` (2026-05-04) | Submodule `sources/references/mission-v0`. Newer than v1; adds the RP2350 and NOEL-V targets. |
| https://github.com/projetoChi2p/fs-nasa-cfs-mission-v1 | `01acb7ee267162786d86c1e75d3f60a83b760e16` (2026-04-20) | Submodule `sources/references/mission-v1`. PolarFire SoC (MPFS) focus, adds hs_app. |
| https://github.com/projetoChi2p/fs-nasa-cfs-osal | v0: `86d2a7592c61fd0f146474ea55a568eae53e23af`, v1: `1b499de4307fc921174dff09658f88613b1e5f3c` | Not a submodule (D11). Merge-base with upstream is exactly our pinned OSAL `a77b5bd`. |
| https://github.com/projetoChi2p/fs-nasa-cfs-psp | v0: `5f99e74e43bb7c8ca52296687c3663d7d4a85cf0`, v1: `6537eb7bc833ec41e5258cef18fcce3b9d566a85` | Not a submodule. Based on PSP `70be39d` (equuleus-rc1+dev40), older than our dev73. |
| https://github.com/projetoChi2p/fs-nasa-cfs-cfe | `e2daf50ac211f15107d4d96f2fe46ce5c71b057f` | Content identical to our pinned cFE `fc8c48a` (empty `git diff`). |
| https://github.com/multica-ai/andrej-karpathy-skills | `2c606141936f1eeef17fa3043a72095b4765b9c2` | Source of the Karpathy guidelines (D2). |

## Relevant files and findings

### OSAL fork

- `src/os/freertos/` — FreeRTOS OS layer: tasks, queues, semaphores, mutexes, timebase, console, files.
  - Uses **dynamic** allocation: `xTaskCreate`, `xQueueCreate`, `xSemaphoreCreate*` and `pvPortMalloc`
    (`os-impl-task.c:77-90`, `os-impl-queue.c:30`, `os-impl-idmap.c:162`, `os-impl-filesys.c:238`).
  - The timebase uses static timer/semaphore objects (`os-impl-timebase.c:399-440`). That is a useful pattern.
- `src/bsp/shared-freertos/src/bsp_start.c` — `main()` creates the BSP main task, then calls `vTaskStartScheduler()`.
- `src/bsp/pc-freertos-linux/CMakeLists.txt` — compiles the FreeRTOS sources, POSIX port and `heap_4.c` into the BSP.

### PSP fork

- `fsw/shared-freertos/src/cfe_psp_start.c:98-151` — embedded `STARTUP_SCR` is written to a RAM disk made by
  `OS_mkfs(NULL, …)`, which allocates dynamically. We adopt the idea with a static buffer (D5).

### Mission repos (`obdh_v1_defs/`)

- `toolchain-i386-freertos-linux.cmake` — simulator build:
  - `-m32`;
  - a vendored FreeRTOS **v10.5.1** POSIX copy;
  - `OSAL_SYSTEM_OSTYPE=freertos`, `OSAL_SYSTEM_BSPTYPE=pc-freertos-linux`, `CFE_SYSTEM_PSPNAME=pc-freertos-linux`;
  - generates `FreeRTOSConfig.h` from `FreeRTOSConfig.h.in` (which has `configSUPPORT_DYNAMIC_ALLOCATION 1`).
- `targets.cmake` — all apps static (`STATIC_APPLIST`, `STATIC_SYMLIST`); `EMBED_FILELIST "STARTUP_SCR,cfestrup.scr"`.
- `cpu1_platform_cfg.h` — reduced memory: SB pool 64 KB, TBL pool 64 KB, CDS 12 KB, RAM disk 128×128 B,
  syslog 1 KB, stacks 2–6 KB.
- `prog_mpfs_openocd_lim.sh`, `prog_stm32_nucleo.sh`, `start_gdb_mpfs.sh` — OpenOCD/gdb programming patterns.

## Detailed review of the OSAL fork (2026-11-03)

Fetched with user approval as the nested submodule `sources/references/mission-v0/osal` at the commit
mission-v0 pins (`86d2a7592c61fd0f146474ea55a568eae53e23af`), over HTTPS (`-c url.…insteadOf`, no config change).
Top-level pins unchanged. 72 commits on top of upstream `a77b5bd`.

**Licence and origin.** The repo is Apache-2.0. Some `src/os/freertos` files carry UFRGS 2024, "Patrick Paul 2021,
MIT-0" and NASA headers; others (e.g. `os-impl-task.c`) have no header. The origin is mixed; reuse would need a
per-file check.

**Layer layout** (`src/os/freertos/`, about 6,000 lines; filesys alone is 1,587): tasks, queue, binsem, countsem,
mutex, idmap, timebase, clock, common, console, errors, heap, file, dirs, filesys. Loader, network, sockets,
select, condvar and shell use OSAL's `portable/os-impl-no-*.c`.

**Reusable ideas**
- Timebase (`os-impl-timebase.c`): a static FreeRTOS software timer (`xTimerCreateStatic`) gives a static binary
  semaphore. A helper task waits on it and runs `OS_TimeBase_CallbackThread`. One-shot start time first, then
  auto-reload at the interval.
- Global table locks (`os-impl-idmap.c`): one FreeRTOS mutex per OSAL object type. `OS_WaitForStateChange_Impl`
  unlocks, delays `attempts²` ticks (max 100), then relocks.
- Clock (`os-impl-clock.c`): local time = tick count since an epoch, plus an OSAL time offset set by
  `OS_SetLocalTime_Impl`.
- BSP (`shared-freertos/src/bsp_start.c`): `main()` creates a BSP main task, then starts the scheduler. The task
  runs `OS_Application_Startup()` and `OS_Application_Run()`. `OS_IdleLoop_Impl` suspends the task;
  `OS_ApplicationShutdown_Impl` resumes it.
- Task ID lookup (`os-impl-task.c`): the FreeRTOS task name is the decimal OSAL ID, and `OS_TaskGetId_Impl`
  parses it back with `atol(pcTaskGetName(NULL))`. It works, but depends on name length. FreeRTOS
  thread-local storage would be more robust.

**Defects found (to avoid in our layer)**
- Dynamic allocation: `xTaskCreate` for tasks with no stack given and for the timebase helper task;
  `pvPortMalloc(StaticTask_t)` even when a stack is given; `xQueueCreate`; `xSemaphoreCreate*` for binsem, mutex
  and table locks; `pvPortMalloc` for the RAM-disk image (`os-impl-filesys.c:238`) and directory search
  (`os-impl-dirs.c:206`); FreeRTOS+FAT `ffconfigMALLOC = pvPortMalloc`; the simulator BSP links `heap_4.c`.
- Queues: `OS_QueuePut_Impl` ignores `size` and copies `max_size` bytes, so it can read past the caller's buffer.
  `OS_QueueGet_Impl` always reports `size_copied = max_size`. `OS_QueueGetInfo_Impl` is a no-op.
- Binsem: the table is sized `OS_MAX_COUNT_SEMAPHORES`, not `OS_MAX_BIN_SEMAPHORES`. `OS_BinSemFlush_Impl`
  returns `OS_ERR_NOT_IMPLEMENTED`, and bin-sem-test uses flush.
- Mutex: created with `xSemaphoreCreateMutex` (non-recursive) but taken and given with the `*Recursive` calls.
- Tasks: `OS_TaskDelay_Impl` truncates (`ms / portTICK_PERIOD_MS`). `OS_TaskIdMatchSystemData_Impl` returns
  `OS_ERROR` from a `bool` function, which is always true.
- Priority mapping (`OS_FreeRTOS_MapOsalPriority`, in the BSP): OSAL priority 0 maps to `configMAX_PRIORITIES`,
  one past the highest valid priority. OSAL 255 maps to the idle priority 0.
- Layering: the OS layer includes a BSP header (`generic_freertos_bsp_internal.h`) to register the timebase task;
  the OS priority map lives in the BSP.
- File systems: Xilinx MFS, FreeRTOS+FAT or ChaN FatFs. All are third-party and none is pinned here.

**Changes outside the FreeRTOS layer**
- `src/tests/bin-sem-test`, `count-sem-test`: task priorities reordered and the priority check skipped when
  priorities are "not in effect". Possibly a backport of newer upstream tests (not verified).
  `timer-test`: zeroes `timer_counter[i]`.
- `ut_assert`: only include/comment changes. Note: `UtList_Create` uses `malloc`, so the OSAL test harness needs
  a C heap. Acceptable on the host; a target issue later.
- `unit-tests/CMakeLists.txt`: `oscore-test` disabled on the Nucleo/MPFS boards ("overflows RAM").

## Applicability to SmartFusion2 (Cortex-M3)

- **STM32F767 (Cortex-M7, ARMv7-M):** the linker script and `syscalls.c` structure are applicable. The HAL,
  FPU and cache code are not.
- **PolarFire SoC / RISC-V, NOEL-V, RP2350:** different ISA. Only the tooling and scripting style is informative.

## Open items

- Licence of the forks' new FreeRTOS files: not checked (not needed while D8 holds).
