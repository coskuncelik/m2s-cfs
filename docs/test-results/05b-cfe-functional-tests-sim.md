# Test result — 05b cFE functional tests on the Linux simulator

- Date: 2026-11-05
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs, Intel Xeon Platinum 8370C), gcc 13.3.0, cmake 3.28.3
- Sources (unmodified in `sources/`): cFS `c62b5b9` (cFE equuleus-rc1+dev247, OSAL `a77b5bd`, PSP equuleus-rc1+dev73,
  sample_app, sample_lib), FreeRTOS-Kernel `8a59a4b` (V11.3.2)
- Project code: `scripts/05b-run-cfe-functional-tests-sim.sh`, `config/cfe/native-ft/` (native reference overlay),
  `config/cfe/freertos-posix-sim-ft/` (test mission overlay), `apps/ft_logdump/` (test-only), PSP `/cf` image
  unpacker (`cfe_psp_start.c`), `scripts/lib/cfe-sim.sh` (defs assembly, `/cf` image, two-pass build, link checks),
  link rule in `config/cfe/freertos-posix-sim/arch_build_custom.cmake`. Decision: D29.
- Command: `./scripts/05b-run-cfe-functional-tests-sim.sh --clean` (ROUNDS=3) — exit 0

## Results

| Check | Result |
|---|---|
| Native reference build and run (stock pc-linux, same apps, dynamically loaded) | PASS — 2521 assertions, 2044 PASS, 0 FAIL, 13 MIR, 41 s |
| FreeRTOS build (two passes: tables generated, then embedded in the `/cf` image) | PASS |
| `CFE_PSP_Main` linked; no FreeRTOS heap; `malloc`/`free` only in the host POSIX port (2 of 192 objects) | PASS |
| 3 FreeRTOS rounds: log recovered with matching size; every assertion equal to native except the 1 approved | PASS — 3/3 |
| The 3 rounds give identical assertion lists | yes |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |

| Run | Total | PASS | FAIL | MIR | N/A | Time |
|---|---|---|---|---|---|---|
| Native (stock) | 2521 | 2044 | 0 | 13 | 14 | 41 s |
| FreeRTOS round 1–3 | 2521 | 2043 | 1 | 13 | 14 | 55–56 s |

The comparison is per assertion: status and `file:line` of each of the 2521 assertions, in order. The only
difference is `es_task_test.c:108` (PASS natively, FAIL on FreeRTOS), allowed by D29: it checks that a child task's
local variable lies inside the stack buffer passed to it; on the FreeRTOS POSIX port a task runs on its host
thread's stack (same cause as D22, simulator only). The N/A items are the same on both (6+6 "AddressesAreValid is
false" in es_info_test, 2 msg_api_test items not applicable to the message type).

Regression after the shared changes (`/cf` image, link rule, `nm` checks): `06-run-counter-app-sim.sh --clean` 5/5,
`05-build-cfe-freertos-sim.sh` 5/5, `03-build-freertos-posix-smoke.sh --clean` 10/10,
`ROUNDS=3 04-build-osal-freertos-sim.sh --clean` 75/75 — all exit 0.

## Manual-inspection items (SB performance, informational)

| Item | Native | FreeRTOS simulator |
|---|---|---|
| CPU speed loop count (test calibration) | 2,704,919 | 238,667 |
| SB bulk message test, rate | 299,109 msg/s | 76,773 msg/s |
| SB bulk CMD+TLM threads, rate | 309,085 msg/s | 78,793 msg/s |
| SB bulk multi-pipe, rate | 217,041 msg/s | 33,755 msg/s |

The simulator runs every task on one host CPU (D21) through the POSIX port's signal-based context switches, so
lower rates are expected; the test scales its message count with the calibration loop. Not a pass/fail criterion.

## Found and fixed during the step

1. Static-only link: cFE links its core modules with `--whole-archive` only when a CPU has dynamically loaded apps, so
   `cfe_testcase` (which uses `CFE_TBL_Load` etc.) did not link. Executables now link their libraries in a GNU ld group.
2. Wrong start-up linked: with the group, OSAL's fallback `OS_Application_Startup` (`libosal_bsp`,
   `bsp_default_app_startup.c`) was linked instead of the PSP's; cFE never started and printed nothing. Fixed with
   `-u CFE_PSP_Main`; the scripts now fail if `CFE_PSP_Main` is missing from the image.
3. `nm … | grep -q` under `set -o pipefail` can fail when grep exits early (SIGPIPE). In the "must not contain"
   checks of scripts 03, 04 and 05/06 that could hide a hit; all now grep captured output. (Earlier results stand:
   those checks had found nothing, and the malloc users had been listed separately.)
4. The console does not carry the complete results (EVS drops event bursts): first native run showed 3 of 6 FAIL
   lines. Results now come from `/cf/cfe_test.log`, printed by `ft_logdump` as prefixed lines; a first version that
   printed raw chunks got two cFE TIME event lines mixed in (caught by the size check).
5. The first native reference without `sample_app` had 6 FAILs (`CFE_TBL_Share` of `SAMPLE_APP.ExampleTable`): the
   tests need sample_app's table. Both runs now include sample_lib and sample_app.

## Notes and open points

- Facts: on FreeRTOS, cFE's functional tests give the stock result for every assertion but one simulator-only item.
- Statically linked apps and libraries (cfe_assert, sample_lib, sample_app, cfe_testcase) start through OSAL's
  static loader; table files reach `/cf` through the embedded image.
- Not covered: processor resets, app reload from a file (the static loader ignores the file), the board.
- The native phase uses the stock pc-linux PSP, which creates SysV shared memory segments on the host (as step 02).

## Re-run 2026-11-06: assertion order in the multi-task SB tests (D29 item 6)

- Repeated runs on the same Docker host: `--clean` ×3 rounds passed twice, then one run failed (exit 1). In round 1
  the only change was the order of two passing lines, `PASS sb_performance_test.c:236` before
  `PASS sb_performance_test.c:183`; results and totals were identical (2521 / 2043 PASS / 1 FAIL known / 13 MIR).
  Cause: "2 Thread Bulk Transfer" starts `CmdXmit` and `TlmXmit` at the same priority (150); each records one
  assertion when it starts, and which goes first depends on where the 1 ms time slice ends. The strict in-order
  comparison then failed that round and, through the round-vs-round check, the other two.
- Fix (approved): in the tests "2 Thread Bulk Transfer" and "4 Thread Bulk Transfer" the assertions are compared as
  a set (same lines and results, any order); order stays strict everywhere else. Checked on the saved logs: only
  assertion lines 1283–1327 (the 48 assertions of these two tests) change position, and a PASS turned into FAIL
  inside them is still reported.
- `ROUNDS=10 ./scripts/05b-run-cfe-functional-tests-sim.sh --clean`: all 10 rounds equal to native except the known
  one; round 7 had the swapped order (236 before 183) and passed. (That run ended with exit 2 only because the
  script's header comment was edited while it ran; bash then misread the rest of the file.)
- `ROUNDS=10 ./scripts/05b-run-cfe-functional-tests-sim.sh` (script unchanged during the run): exit 0, 10/10 rounds
  passed (53–57 s each); all 10 in the usual order (183 before 236).
- The swap was seen in 2 of 23 FreeRTOS rounds on this host. A slower or busier host (e.g. the VirtualBox VM) is
  expected to see it more often; VirtualBox failures reported by the user are not analysed yet (logs pending).
