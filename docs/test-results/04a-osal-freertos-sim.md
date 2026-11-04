# Test result — 04a OSAL FreeRTOS layer on the Linux simulator

- Date: 2026-11-04 (new host; the earlier 04a runs on the VirtualBox VM are summarised below)
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs, Intel Xeon Platinum 8370C), kernel
  6.8.0-1064-azure, glibc 2.39, gcc 13.3.0, cmake 3.28.3
- Sources (unmodified in `sources/`): OSAL `a77b5bd75d611670a6292f0a68922274d3dbf33f`,
  FreeRTOS-Kernel `8a59a4b19bf72977819feddef87d41163ced7004` (V11.3.2)
- Project inputs: `osal-ext/freertos/` (OS layer), `osal-ext/freertos-posix-sim/` (BSP),
  `config/osal/freertos-posix-sim/osal_config.cmake`, `config/freertos/posix/FreeRTOSConfig.h`,
  `cmake/freertos-posix-kernel.cmake`, `patches/freertos-kernel/0001-posix-port-tick-catch-up.patch`
- Command: `ROUNDS=3 ./scripts/04-build-osal-freertos-sim.sh --clean` — exit 0, 5 min 52 s
- No cFE. The tests are OSAL's own functional tests from `sources/cFS/osal/src/tests/`, built unchanged.
  Design of the layer: `docs/ARCHITECTURE.md`.

## Results

| Check | Result |
|---|---|
| Clean configure and build of OSAL + 12 tests (project code `-Wall -Wextra -Werror`) | PASS |
| No FreeRTOS heap or dynamic object creation in any test binary (`nm`: no `pvPortMalloc`, `vPortFree`, `xTaskCreate`, `xQueueGenericCreate`, `xQueueCreateMutex`, `xQueueCreateCountingSemaphore`, `xTimerCreate`, `xEventGroupCreate`) | PASS |
| 12 tests × 3 rounds; every run exits 0 with no FAIL/TSF/TTF (except D22), 90 s limit per run | PASS — 36/36 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean after the build | PASS |

Per test (identical in all 3 rounds):

| Test | Assertions | Result |
|---|---|---|
| osal-core-test | 622 | 621 PASS, 1 FAIL tolerated (02.003, D22) |
| queue-test | 41 | PASS |
| mutex-test | 14 | PASS |
| bin-sem-test | 77 | PASS |
| bin-sem-timeout-test | 12 | PASS |
| count-sem-test | 81 | PASS |
| count-sem-timeout-test | 19 | PASS |
| idmap-api-test | 100 | PASS |
| sem-speed-test | 10 | PASS (work counters ≈ 265k–279k per task in 5 s) |
| timer-test | 29 | PASS |
| time-base-api-test | 44 | PASS |
| timer-add-api-test | 55 | PASS |

**The tolerated failure (D22):** `osal-core-test.c:145` checks that a local variable of a task created with a
caller-supplied stack lies inside that stack. On the FreeRTOS POSIX port every task runs on a host pthread
stack, so this cannot pass on the simulator. Script 04 tolerates exactly this one assertion in this one test.
It must pass on the Cortex-M3.

## Dynamic memory evidence (host build)

`nm -u` over every object in `build/osal-freertos-sim` — only these reference `malloc`/`free`:

| Object | Why | Side |
|---|---|---|
| `port.c` (patched POSIX port), `wait_for_event.c` | FreeRTOS POSIX port thread/event bookkeeping | host only |
| `ut_assert` `utlist.c`, `utstubs.c` | OSAL test harness | host only (test code) |

No object of `osal-ext/freertos/`, `osal-ext/freertos-posix-sim/` or OSAL's shared layer references them.
This is evidence about the project code, not about the embedded target: the `malloc`/`_sbrk` trap and link-map
check for the Cortex-M3 image are still to do (`CLAUDE.md`, engineering rules).

## sem-speed-test hang (closed, D24)

- On the old VirtualBox VM (2026-11-03/04) sem-speed-test hung once in about 35 runs (round 3 of a full run);
  25 later runs and 12 runs under gdb were clean. No output or stack trace of the hung run was kept.
- On the new host: 3/3 in script 04, then 200 untraced runs in a loop — 200 passed, 0 failed, 0 hung,
  5.04–5.14 s each. At 1 in 35, 200 clean runs has a probability of about 0.3 %.
- Code review: a lost wake-up between the two worker tasks cannot hang this test, because the main task still
  wakes after its 5 s delay and deletes them (the work counters are non-zero). A hang must be on the main task's
  path (delay, task delete, semaphore delete, teardown, exit). No bug was found in `os-impl-sem.c`,
  `os-impl-tasks.c` or the port's cancel path (`vPortCancelThread`; its event mutex is robust).
- Decision: closed as a host artefact (VM starvation, see D14, D15, D21); reopen if it appears on any other host
  or on the Cortex-M3.

## Notes and open points

- Facts: all 12 tests pass on the simulator; the static-allocation checks above pass.
- Reasoned conclusion: the layer's behaviour matches the OSAL API for tasks, queues, semaphores, mutexes, id
  maps, time bases and timers, as far as these 12 tests cover it.
- Not tested yet: files and file systems (stubbed, 04b), condition variables, select, shell, symbol table,
  network, dynamic loader (OSAL "no-*" stubs), and OSAL's API-exhaustive `src/unit-tests` (04c).
- Not tested: any non-x86 target. Timing tolerances in the OSAL tests were met on this host only.
- Debugging on this host: gdb cannot attach to a running process (no capabilities in the container); it can
  run a program as its own child. Core dumps are not reachable.
