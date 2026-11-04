# Project status

Last updated: 2026-11-04. Read this first when resuming. Rules: `CLAUDE.md`. Decisions: `docs/DECISIONS.md`.

## Steps

| Step | State | Evidence |
|---|---|---|
| 00 basic setup (pinned submodules) | done | `logs/00-basic-setup.log` |
| 01 host prerequisites check | done | `logs/01-check-prerequisites.log` |
| 02 stock native-POSIX cFS baseline | done | `docs/test-results/02-native-baseline.md` |
| 03 FreeRTOS POSIX smoke test (patch 0001, idle hook) | done; re-run 2026-11-04 after config changes: 10/10 PASS | `docs/test-results/03-freertos-posix-smoke.md` |
| **04a OSAL FreeRTOS core layer + simulator BSP** | **in progress** (approved) | `scripts/04-build-osal-freertos-sim.sh`, `logs/04-*` |
| 04b static RAM file system (D19) | not started; needs approval | — |
| 04c full OSAL functional test set | not started; needs approval | — |
| PSP, cFE on the simulator, counter_app, SmartFusion2 target | not planned in detail yet | — |

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
- Clock: tick count + epoch. Console: async static task -> BSP `write(1)`. Files: `os-impl-no-filesys.c`
  placeholder (OS_ERR_NOT_IMPLEMENTED) until 04b.
- BSP: pins the process to one CPU (D21), static main task, `exit()` with the test status.

Last full run (2026-11-03/04, 3 rounds): all 12 tests pass except
- osal-core-test 02.003 — known simulator limitation, now tolerated by the script (D22);
- **sem-speed-test hung once (round 3)**, about 1 in 35 runs overall; 25 later runs and 12 runs under gdb were clean.

### Next actions for 04a (approved: "investigate before closing")
1. Find the sem-speed-test hang. Plan: a debug-build-only watchdog that, when a test stalls, prints scheduler
   state (`xTickCount`, `uxSchedulerSuspended`, current task) and each semaphore's value and waiter list; run
   sem-speed-test in a long loop. To attach gdb to a hung run without slowing it, use `tools/debug/ptrace-launch.c`
   (ptrace_scope=1 on this host). Suspects: a race in `os-impl-sem.c`, or the POSIX port under ~170k switches/s.
2. Re-run `./scripts/04-build-osal-freertos-sim.sh --clean` (ROUNDS=3 or more) until clean; not yet run after
   the D22 change.
3. Write `docs/test-results/04a-osal-freertos-sim.md`, `docs/ARCHITECTURE.md` (OSAL layering), README line.
4. Ask for approval of 04b.

## Environment facts that matter
- Host is a 2-vCPU VirtualBox VM: `usleep(1000)` ≈ 2.7 ms; busy threads can starve others for seconds
  (D14, D15, D21). Suspect this first for any simulator hang or timing drift.
- `kernel.yama.ptrace_scope=1`: gdb can only attach to its own children (see `tools/debug/`).
- Git: public repo https://github.com/coskuncelik/m2s-cfs, branch `main` (D23). Commit/push only with approval.
