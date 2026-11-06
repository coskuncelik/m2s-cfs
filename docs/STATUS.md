# Project status

Last updated: 2026-11-06. Read this first when resuming. Rules: `CLAUDE.md`. Decisions: `docs/DECISIONS.md`.

## Steps

| Step | State | Evidence |
|---|---|---|
| 00 basic setup (pinned submodules) | done | `logs/00-basic-setup.log` |
| 01 host prerequisites check | done | `logs/01-check-prerequisites.log` |
| 02 stock native-POSIX cFS baseline | done | `docs/test-results/02-native-baseline.md` |
| 03 FreeRTOS POSIX smoke test (patch 0001, idle hook) | done; re-run 2026-11-04 after config changes: 10/10 PASS; re-run on new host 2026-11-04: 10/10 PASS | `docs/test-results/03-freertos-posix-smoke.md` |
| 04a OSAL FreeRTOS core layer + simulator BSP | done (closed 2026-11-04) | `docs/test-results/04a-osal-freertos-sim.md` |
| **04b static RAM file system (D19, D25)** | **closed 2026-11-04 (user approved)** | `docs/test-results/04b-ramfs-sim.md` |
| **04c full OSAL functional test set (D26)** | **closed 2026-11-04 (user approved)** | `docs/test-results/04c-osal-unit-tests-sim.md` |
| **05 PSP and cFE core on the simulator (D27)** | **closed 2026-11-04 (user approved)** | `docs/test-results/05-cfe-freertos-sim.md` |
| **06 counter_app on the simulator (D28)** | **closed 2026-11-05 (user approved)** | `docs/test-results/06-counter-app-sim.md` |
| **05b cFE functional tests on the simulator (D29)** | **closed 2026-11-05 (user approved)** | `docs/test-results/05b-cfe-functional-tests-sim.md` |
| **07 shareable bundle `cfs-m2s-freertos` (D30)** | **closed 2026-11-05 (user approved); published 2026-11-05: github.com/coskuncelik/cfs-m2s-freertos (public), commit `d48850f`** | `docs/test-results/07-export-bundle.md` |
| **08 cross-build for Cortex-M3 (D31)** | **closed 2026-11-05 (user approved)** | `docs/test-results/08-cfe-arm-cm3-build.md` |
| 09 run on an emulated Cortex-M3 (QEMU, e.g. mps2-an385; memory sizes to be checked) | future work (user, 2026-11-05); needs approval to install qemu-system-arm | — |
| SmartFusion2 target | not planned in detail yet (blocked by D7) | — |

## 04a: where it stands

Code: `osal-ext/freertos/` (OS layer, 12 files), `osal-ext/freertos-posix-sim/` (BSP),
`config/osal/freertos-posix-sim/osal_config.cmake`, `cmake/freertos-posix-kernel.cmake` (shared kernel build),
`config/freertos/posix/FreeRTOSConfig.h` (32 priorities, 2 TLS pointers, 2 notification slots).

Design in short (details in file headers):
- Tasks: static TCB pool + stack pools in size classes (D18); self-exited tasks become suspended "zombies" and are
  reclaimed by the next task create; OSAL id in TLS[0]; priority map OSAL 0..255 -> FreeRTOS 30..1.
- Binary/counting semaphores: own core (`os-impl-sem.c`), priority-ordered waiter list on the waiters' stacks,
  task notifications (index 1); supports flush. Mutexes: static recursive mutexes.
- Queues: static FreeRTOS queue per slot, items = [length][payload], staging buffers under static mutexes.
- Timebase: static helper task sleeping to absolute tick deadlines; cooperative delete.
- Clock: tick count + epoch. Console: async static task -> BSP `write(1)`. Files: static RAM file system since
  04b (`os-ramfs.c` + `os-impl-filesys.c`, D25).
- BSP: pins the process to one CPU (D21), static main task, `exit()` with the test status.

Last full run (2026-11-03/04, 3 rounds): all 12 tests pass except
- osal-core-test 02.003 — known simulator limitation, now tolerated by the script (D22);
- **sem-speed-test hung once (round 3)**, about 1 in 35 runs overall; 25 later runs and 12 runs under gdb were clean.

Re-run on the new host (2026-11-04, Docker, 2 vCPU Xeon Platinum 8370C, Ubuntu 24.04.5, gcc 13.3.0, cmake 3.28.3):
steps 00–03 all exit 0 (03: 10/10 PASS); `ROUNDS=3 ./scripts/04-build-osal-freertos-sim.sh --clean` exit 0,
36/36 runs passed (02.003 tolerated per D22), sem-speed-test clean in all 3 rounds. This is the first `--clean`
run after the D22 change.

sem-speed-test hang: **closed as a host artefact (D24)**. 200 untraced runs in a loop on the new host all passed;
no code change. Reopen if it appears again.

### Next actions for 04a (approved: "investigate before closing")
1. ~~Find the sem-speed-test hang~~ — closed as a host artefact (D24).
2. ~~Re-run script 04 with `--clean` after D22~~ — done 2026-11-04 on the new host: 36/36 passed.
3. ~~Write the 04a result, architecture doc and README line~~ — done 2026-11-04: `docs/test-results/04a-osal-freertos-sim.md`,
   `docs/ARCHITECTURE.md`, README updated.
4. ~~Ask for approval of 04b~~ — approved and implemented 2026-11-04.

## 04b: result

`ROUNDS=3 ./scripts/04-build-osal-freertos-sim.sh --clean` exit 0: core test 109/109, 42/42 OSAL runs
(file-api-test 140/140, file-sys-add-fixed-map-api-test 8/8). Closed and committed 2026-11-04 (user approved).
04c plan approved 2026-11-04 (recommendations: include the 5 skipped `src/tests`, include osal_loader_UT,
leave out `unit-test-coverage`, fix clear bugs directly).

## 04c: result

`ROUNDS=3 ./scripts/04-build-osal-freertos-sim.sh --clean` exit 0: 25 programs × 3 rounds, 75/75 passed, same
summaries in every round. One bug fixed (too few file entries on small RAM disks); simulator pool raised to
1 MiB; 12 assertions tolerated for features off by configuration (D26). Closed and committed 2026-11-04 (user
approved).

## 05: result

Plan approved 2026-11-04 (recommendations: build-time PSP copy, own `timebase_freertos`, embedded startup
script now, cFE functional tests later in 05b). `./scripts/05-build-cfe-freertos-sim.sh --clean` exit 0: stock
cFE core (no apps) builds on the FreeRTOS OSAL with the project PSP, reaches OPERATIONAL with the embedded startup
script, 5/5 runs of 20 s; no `malloc`/`free` outside the host POSIX port. Steps 03 and 04 re-run after the kernel
CMake change (`_GNU_SOURCE=`). Closed and committed 2026-11-04 (user approved).

## 06: result

Plan approved 2026-11-05; user chose `OS_printf` for output (D28 updates D6). `./scripts/06-run-counter-app-sim.sh
--clean` exit 0: counter_app linked statically, started by the embedded script, prints "counter_app initialized"
and "counter_app counter: N" once per second; 5/5 runs, 19 counts each, drift under 30 µs over 18 s. A plain
`OS_TaskDelay(1000)` drifted 1 ms/s; the app now uses fixed deadlines. Scripts 05 (5/5) and 04 (75/75) re-run.
Closed and committed 2026-11-05 (user approved).

## 05b: result

Plan approved 2026-11-05 (recommendations: native reference, `/cf` tar image, separate test mission, stop before
new tolerances). `./scripts/05b-run-cfe-functional-tests-sim.sh --clean`: native 2521 assertions, 2044 PASS, 0 FAIL;
FreeRTOS 3 rounds identical, every assertion equal to native except `FAIL es_task_test.c:108` (approved, D29).
Fixed on the way: static-only link order, wrong `OS_Application_Startup` linked, `nm | grep -q` under pipefail in
scripts 03/04/05/06. Closed and committed 2026-11-05 (user approved). Next: plan the next step with the user (SmartFusion2 target needs D7 sources).

## 07: result

Plan approved 2026-11-05 (option A: generated bundle in the stock cFS layout; name `cfs-m2s-freertos`; "m2s" kept
in names; Apache-2.0 for our code; simulator + counter_app only; publish later as a GitHub repository).
`./scripts/07-check-bundle.sh` exit 0: reproducible tree, 3149 upstream files byte-identical, no private
references, a fresh clone builds with stock `make prep && make && make install`, static-memory check, counter_app
5/5. Regression 06/05/03/04 pass. Fixed on the way: `tables/staging` removed by cFE's `prepare()` (deferred mkdir,
CMake >= 3.19). Closed and committed 2026-11-05 (user approved). Next: push the bundle to the GitHub
repository https://github.com/coskuncelik/cfs-m2s-freertos (address given by the user). Pushed by the user 2026-11-05 (the codespace token cannot write to it; a fine-grained token was used), public, `main` = `d48850f` (source revision `76de858`), verified with git ls-remote.

## 08: result

Plan approved 2026-11-05 ("use script 01 approach, go on with your recommendations"); toolchain installed by the
user (gcc-arm-none-eabi 13.2.1, binutils 2.42, newlib-nano 4.4.0). `./scripts/08-build-cfe-arm-cm3.sh --clean`
exit 0: ARMv7-M Thumb-2 image, entry and vectors checked, link map, no allocator or heap linked (board traps,
approved after newlib's `malloc` was found linked), footprint text 212077 / data 1296 / bss 7568804 on the
placeholder board `boards/link-check`. Regression 06/05/04/07-check pass. The published bundle is now one line
behind (`os-impl-filesys.c` 32-bit guard; no behaviour change on 64-bit); republish only with approval.
Closed and committed 2026-11-05 (user approved). Next: step 09 (QEMU Cortex-M3) when the user decides.

## 05b re-run (2026-11-06)

One of three repeated `--clean` runs failed: two passing assertions of the SB "2 Thread Bulk Transfer" test (equal
priority child tasks) were recorded in the other order. Fixed with user approval: the two multi-task SB tests are
compared as a set, the order is strict elsewhere (D29 item 6). `ROUNDS=10` exit 0, 10/10 (and 10/10 in the run that
contained a swapped round). Committed 2026-11-06 (user approved). Open: the user's VirtualBox 05b failures (logs to be sent).

## Environment facts that matter
- Current host (since 2026-11-04): Docker container, 2 vCPU Xeon Platinum 8370C, Ubuntu 24.04.5. No effective
  capabilities: gdb cannot attach to a running process at all (`tools/debug/ptrace-launch.c` does not help here);
  gdb can still run a program as its own child. Core dumps go to the host's apport pipe and are not reachable.
- Previous host was a 2-vCPU VirtualBox VM: `usleep(1000)` ≈ 2.7 ms; busy threads could starve others for seconds
  (D14, D15, D21). Suspect host starvation first for any simulator hang or timing drift.
- Git: public repo https://github.com/coskuncelik/m2s-cfs, branch `main` (D23). Commit/push only with approval.
