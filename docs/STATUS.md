# Project status

Last updated: 2026-11-04. Read this first when resuming. Rules: `CLAUDE.md`. Decisions: `docs/DECISIONS.md`.

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
| 05b cFE functional tests, counter_app, SmartFusion2 target | not planned in detail yet | — |

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
CMake change (`_GNU_SOURCE=`). Closed and committed 2026-11-04 (user approved). Next (when resuming): choose
05b (cFE functional tests) or `counter_app`; each needs its own plan and approval.

## Environment facts that matter
- Current host (since 2026-11-04): Docker container, 2 vCPU Xeon Platinum 8370C, Ubuntu 24.04.5. No effective
  capabilities: gdb cannot attach to a running process at all (`tools/debug/ptrace-launch.c` does not help here);
  gdb can still run a program as its own child. Core dumps go to the host's apport pipe and are not reachable.
- Previous host was a 2-vCPU VirtualBox VM: `usleep(1000)` ≈ 2.7 ms; busy threads could starve others for seconds
  (D14, D15, D21). Suspect host starvation first for any simulator hang or timing drift.
- Git: public repo https://github.com/coskuncelik/m2s-cfs, branch `main` (D23). Commit/push only with approval.
