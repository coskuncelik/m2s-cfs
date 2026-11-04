# Test result — 05 PSP and cFE core on the Linux simulator

- Date: 2026-11-04
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs, Intel Xeon Platinum 8370C), kernel
  6.8.0-1064-azure, glibc 2.39, gcc 13.3.0, cmake 3.28.3
- Sources (unmodified in `sources/`): cFS `c62b5b9` (cFE equuleus-rc1+dev247, OSAL `a77b5bd`, PSP
  equuleus-rc1+dev73), FreeRTOS-Kernel `8a59a4b` (V11.3.2)
- Project code: `psp-ext/fsw/freertos-posix-sim/` (PSP platform), `psp-ext/fsw/modules/timebase_freertos/`
  (PSP module), `config/cfe/freertos-posix-sim/` (mission config), `scripts/05-build-cfe-freertos-sim.sh`;
  `cmake/freertos-posix-kernel.cmake` gained `_GNU_SOURCE=`. Decision: D27.
- Command: `./scripts/05-build-cfe-freertos-sim.sh --clean` (RUNS=5, RUN_SECONDS=20) — exit 0, 1 min 56 s

## Results

| Check | Result |
|---|---|
| Clean build + install of `core-cpu1` (cFE flags `-std=c99 -pedantic -Werror`; project code also `-Wextra`) | PASS |
| No FreeRTOS heap or dynamic FreeRTOS object creation in `core-cpu1` | PASS |
| `malloc`/`free` referenced only by the host POSIX port (`port.c`, `wait_for_event.c`) — no cFE, PSP, OSAL or project object | PASS |
| 5 runs × 20 s: still running at the end (exit 124 from `timeout`), all 11 expected lines, no error line | PASS — 5/5 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |
| Regression after the kernel CMake change: `03-build-freertos-posix-smoke.sh --clean`, `ROUNDS=3 04-build-osal-freertos-sim.sh --clean` | PASS — 10/10 and 75/75 |

Expected lines checked in every run: startup script installed in `/cf`, FreeRTOS time base in use, power-on
reset, EVS/SB/ES/TIME/TBL initialized, startup script opened from `/cf`, ES entering OPERATIONAL state,
TIME "Stop FLYWHEEL" (1 Hz time processing running). Error lines: any line matching
`error|fail|panic|cannot|too small|abort|assert` (case-insensitive). Checked against an edited log: a missing
startup script is caught both as missing lines and as an error line.

## Start-up (run 1, abridged)

```
CFE_PSP: reserved memory: 3406464 of 4194304 static bytes used
CFE_PSP: embedded startup script installed as /cf/cfe_es_startup.scr (213 bytes)
CFE_PSP: using the FreeRTOS tick (1000 Hz) as CFE timebase
1980-001-00:00:00.00200 CFE_ES_SetupResetVariables: POWER ON RESET due to Power Cycle (Power Cycle).
...
EVS Port1 1980-012-14:03:20.05099 66/1/CFE_EVS 1: cFE EVS Initialized: ...
EVS Port1 1980-012-14:03:20.15299 66/1/CFE_SB 1: cFE SB Initialized: ...
EVS Port1 1980-012-14:03:20.25498 66/1/CFE_ES 1: cFE ES Initialized: ...
EVS Port1 1980-012-14:03:20.25498 66/1/CFE_ES 2: cFS Versions: cfe equuleus-rc1+dev247, osal equuleus-rc1+dev117, psp equuleus-rc1+dev73-m2s-freertos. cFE chksm 65535
EVS Port1 1980-012-14:03:20.35699 66/1/CFE_TIME 1: cFE TIME Initialized: ...
EVS Port1 1980-012-14:03:20.45899 66/1/CFE_TBL 1: cFE TBL Initialized: ...
1980-012-14:03:20.51000 CFE_ES_Main: CFE_ES_Main entering CORE_READY state
1980-012-14:03:20.51000 CFE_ES_StartApplications: Opened ES App Startup file: /cf/cfe_es_startup.scr
1980-012-14:03:20.51000 CFE_ES_Main: CFE_ES_Main entering APPS_INIT state
1980-012-14:03:20.51000 CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state
EVS Port1 1980-012-14:03:21.00000 66/1/CFE_TIME 21: Stop FLYWHEEL
```

The core start-up sequence and its timing (services about 100 ms apart, OPERATIONAL at 14:03:20.5, flywheel stop
at 14:03:21) match the stock native baseline (`02-native-baseline.md`). The baseline also loads apps and the cFE
functional test; this run has none. `cFE chksm 65535` is cFE's value when the PSP reports no text segment
(`CFE_PSP_GetCFETextSegmentInfo` not implemented, as pc-linux).

## Static memory (simulator settings)

`size core-cpu1`: text 485,393, data 5,672, bss 7,632,688 bytes. Largest static objects:

| Object | Bytes | Owner |
|---|---|---|
| `CFE_PSP_FreeRTOS_ReservedMem` | 4,194,304 | PSP reserved memory (3,406,464 used: 2 MiB `/ram`, 1 MiB user area, 128 KiB CDS, reset area) |
| `OS_FreeRTOS_RamPool` | 1,048,576 | RAM-disk pool (D26) |
| `CFE_TBL_Global` | 580,640 | cFE |
| `OS_impl_queue_table` | 555,008 | OSAL queues (8 KiB storage per queue) |
| `CFE_SB_Global` | 528,832 | cFE (software bus buffer pool) |
| task stack pools | 458,752 | OSAL tasks (D18) |

All are compile-time settings; the board needs much smaller values (open point).

## Notes and open points

- Facts: cFE core runs on FreeRTOS on the simulator from a static image; cFE, PSP and OSAL code do not
  reference `malloc`/`free`.
- Not tested: apps (`counter_app`, next step), cFE's functional tests (step 05b), processor resets (and so the
  `/ram` startup script, which cFE tries only on a processor reset).
- The time base has 1 ms resolution (FreeRTOS tick), below the 1 µs the PSP API recommends (D27).
- Liveness is shown only by the process still running after 20 s and the 1 Hz TIME message; with no apps
  there is no periodic output.
