# Project status

Last updated: 2026-11-13. Read this first when resuming. Rules: `CLAUDE.md`. Decisions: `docs/DECISIONS.md`.

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
| **09 cFE + counter_app on an emulated Cortex-M3, QEMU mps2-an385 (D32)** | **closed 2026-11-06 (user approved)** | `docs/test-results/09-cfe-arm-cm3-qemu.md` |
| **09b OSAL functional tests on the emulated Cortex-M3, QEMU (D33)** | **closed 2026-11-06 (user approved)** | `docs/test-results/09b-osal-tests-arm-cm3-qemu.md` |
| **09c cFE functional tests on the emulated Cortex-M3, QEMU (D34)** | **closed 2026-11-07 (user approved)** | `docs/test-results/09c-cfe-functional-tests-arm-cm3-qemu.md` |
| **07b bundle: simulator + QEMU Cortex-M3 configurations, history kept (D35)** | **closed 2026-11-09 (user approved); bundle published 2026-11-09: main = `ed41597`, tags `v1-sim`, `v2-sim-qemu`** | `docs/test-results/07b-export-bundle-qemu.md` |
| **10 memory sizing of the Cortex-M3 flight image (D36)** | **closed 2026-11-12 (user approved; option A); 07-check-bundle failure left open (pre-existing export issue)** | `docs/test-results/10-memory-sizing.md` |
| 11 counter_app features (11a–11d, simulator) | **closed 2026-11-13 (user approved): 11a (D37), 11b (D38), 11c (D39), 11d (D40)** | `docs/test-results/11a-counter-app-events-files.md`; roadmap below |
| 12 TC/TM over UDP with a Python ground station (12a–12c, simulator) | **closed 2026-11-13 (user approved): 12a (D41), 12b (D42), 12c (D43)** | see "Roadmap: steps 11 and 12" |
| **13 bundle v3: commands and telemetry (13a), publish `v3-tctm-sim` (13b)** | **13a closed 2026-11-13 (D44, user approved); 13b in preparation** | `docs/test-results/13a-bundle-tctm.md` |
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
Closed and committed 2026-11-05 (user approved).

## 05b re-run (2026-11-06)

One of three repeated `--clean` runs failed: two passing assertions of the SB "2 Thread Bulk Transfer" test (equal
priority child tasks) were recorded in the other order. Fixed with user approval: the two multi-task SB tests are
compared as a set, the order is strict elsewhere (D29 item 6). `ROUNDS=10` exit 0, 10/10 (and 10/10 in the run that
contained a swapped round). Committed 2026-11-06 (user approved). Open: the user's VirtualBox 05b failures (logs to be sent);
see "Known issues on the VirtualBox VM host" below.

## 09: result

Plan approved 2026-11-06 (QEMU `mps2-an385`, `boards/qemu-mps2-an385` + `boards/common-cm3`, data in PSRAM with
sizes unchanged, simulator timing limits) plus boot-loader quiesce code in `Reset_Handler`. QEMU 8.2.2 and
gdb-multiarch were already installed. `./scripts/09-run-cfe-arm-cm3-qemu.sh --clean` exit 0: memory map checked,
image and no-heap checks, 5/5 runs (cFE OPERATIONAL, counter 1..19, max deviation 2.4–5.9 ms, drift ≤ 3.1 ms, no
`M2S` trap/fault lines). QEMU's default timing ran 6–13 % slow (lost SysTicks on a busy host); fixed with
`-icount shift=auto,sleep=on,align=on`, limits unchanged (approved). Trap path checked with gdb (`malloc` →
`M2S TRAP` line). Regression 08/06/05/07-check pass. Closed and committed 2026-11-06 (user approved). Next: possible 09b (OSAL
and cFE functional tests on QEMU).

## 09b: result

Plan approved 2026-11-06 (test-only static allocator for ut_assert, semihosting exit on the QEMU board, ROUNDS=3,
cFE functional tests on QEMU later as 09c). `./scripts/09b-run-osal-tests-arm-cm3-qemu.sh --clean` exit 0: 75/75
runs (25 tests x 3); same totals as the simulator except osal-core-test 622/622 — D22's simulator-only failure
passes on the Cortex-M3; only D26's 12 configuration failures allowed. Test heap 184–4496 B of 1 MiB. The flight
allocator-origin check was updated for `alloc_traps.c` (first regression of 08/09 failed on that check only).
Regression 08/09/04/06/05 pass (run 2026-11-06 evening, after the user stopped the first one for time).
Closed and committed 2026-11-06 (user approved). Next: 09c (cFE functional tests on QEMU) when the user decides.

## 09c: result

Plan approved 2026-11-06 (copied functional-test overlay with a drift check, no allowed differences, ROUNDS=3,
comparison code shared with 05b in `scripts/lib/cfe-ft.sh`). `./scripts/09c-run-cfe-functional-tests-arm-cm3-qemu.sh
--clean` exit 0: 3/3 rounds, every one of 2521 assertions equal to native (2044 PASS, 0 FAIL, 13 MIR), no allowed
difference — D29's `es_task_test.c:108` passes on the Cortex-M3. Found and fixed: `.got` outside `.data` in both
board linker scripts (ut_assert is position-independent; its zero GOT made a memset clear the vector table →
lock-up); new `.got` check in scripts 08/09/09c. QEMU default timing for 09c (user chose option A: with -icount a
round took 25 min). Regression 08/09/05b/09b pass. Closed and committed 2026-11-07 (user approved). Next: to be decided with the user (K5 on hold; SF2 board step blocked by D7).

## Fix 2026-11-07: QEMU scripts in a terminal

Scripts 09/09b stopped QEMU when run from an interactive terminal (QEMU reads stdin from timeout's background
process group → SIGTTIN; the agent's shell has no terminal, so it was not seen). Fixed: stdin from /dev/null
(`scripts/lib/counter-check.sh`, script 09b, `scripts/lib/cfe-ft.sh`); empty-counter summary guarded. Verified with
a terminal attached: 09, 09b, 09c, 06, 05b pass. Details: `docs/test-results/09-cfe-arm-cm3-qemu.md`. Running QEMU by hand in the foreground: `docs/QEMU-RUN.md`
(commands checked 2026-11-07: counter_app with Ctrl-A X quit, queue-test ending QEMU with status 0). Committed 2026-11-07 (user approved).

## 07b: result

Plan approved 2026-11-09 (configs `freertos_sim`/`freertos_qemu` selected with MISSIONCONFIG, option A: commit on
top of the published bundle, cloning the public bundle allowed, tags `v1-sim`/`v2-sim-qemu`, no test configs).
`./scripts/07-check-bundle.sh` exit 0: reproducible, upstream untouched, no private references (files and commit
message), a fresh clone builds both configs, counter_app 5/5 on the simulator and 5/5 under QEMU. Fixed on the
way: `<name>_perfids.h` per config; 3 s QEMU warm-up launch in the check (cold start took 5 s). Regression 06/09
pass. Bundle pushed by the user 2026-11-09 (normal push, fine-grained token): github.com/coskuncelik/cfs-m2s-freertos
`main` = `ed41597` on `d48850f`, tags `v1-sim` = `d48850f`, `v2-sim-qemu` = `ed41597`, verified with git ls-remote.
Closed and committed 2026-11-09 (user approved). Next: to be decided with the user (K5 deferred; memory sizing;
SF2 board step blocked by D7).

## 10: where it stands

Plan approved 2026-11-12 (D36: provisional budget data + bss ≤ 512 KiB, flight Cortex-M3 configuration only,
measure first; `-O0` kept). Phase 1: `scripts/10-memory-report.sh` (+ `--ft` under the cFE functional tests,
`scripts/lib/mem-probe-gdb.py`). Phase 2 (D36a) done with option A (bundle sources unchanged, BSP main task
keeps 16 KiB): flight RAM 7.57 MB → 500,396 bytes (95 % of 512 KiB), budget check in scripts 08/09. Verified:
08 (a too-low budget fails), 09 10/10 (two earlier runs 4/5: one late/early console line pair each, host
timing), 10 (stacks ≥ 34 % free), 09c 3/3 equal to native, 09b 75/75, 06 5/5, 05b 3/3.
**Open: `07-check-bundle` exit 1**, not caused by step 10: the bundle's `PORTING.md` records the m2s-cfs
revision, so every commit after the published export changes the bundle; the export then moves the local tag
`v2-sim-qemu` and the check's second export cannot fetch the published tag ("would clobber existing tag").
Fixing it is a change to the step 07 scripts; user decision 2026-11-12: left open for now (fixed later the same day, see "Fix 2026-11-12: 07 export/check"). The generated clone's tag was reset to `ed41597`.
Details: `docs/test-results/10-memory-sizing.md`. Closed and committed 2026-11-12 (user approved).

## Fix 2026-11-12: 07 export/check (D35a)

Plan approved 2026-11-12. `07-export-bundle.sh`: generated clone's tags reset to the published ones; a published
tag is never moved; the PORTING.md revision is the last commit touching the exported inputs. `07-check-bundle.sh`
accepts an unchanged bundle too. Verified: export recovers from a moved local tag; a docs edit leaves the bundle
tree unchanged; `07-check-bundle.sh` exit 0 (both configurations 5/5); published repository unchanged. The export
now makes one local, untagged bundle commit (PORTING.md revision line only), not pushed. Details:
`docs/test-results/07b-export-bundle-qemu.md`. Committed 2026-11-12 (user approved).

## Roadmap: steps 11 and 12 (agreed with the user 2026-11-13, nothing implemented)

Each sub-step still gets its own detailed plan (objective, files, risks, success criteria) and user approval
before implementation. 11a done 2026-11-13 (D37): 06 5/5, failure path detected, 08 RAM 501,940 bytes (95 %);
closed and committed (user approved). 11b done 2026-11-13 (D38): 06 5/5, missing library detected, 08 RAM
501,964 bytes, 07-check-bundle exit 0 on the third run (first: 11a comments with private references, fixed;
second: one QEMU run started late on the host); closed and committed (user approved). Next action: the
plan for 11c. 11c done 2026-11-13 (D39; CDS left as future work, sch_lab only in the project missions):
06 5/5, invalid table rejected, 08 RAM 504,748 bytes (96 %), 09 5/5, 10 (stacks ≥ 34 % free), 07-check-bundle
exit 0 on the rerun (first run: one QEMU late/early pair); closed and committed (user approved). 11d done
2026-11-13 (D40): 06 5/5, a 2000 ms table period rejected by the 1 s check (period comes from the timer), 08 RAM
504,764 bytes (96 %), 09 5/5, 10 (worker 83 % stack free), 07-check-bundle exit 0; closed and committed (user
approved). 12a plan approved 2026-11-13. Phase 1 done (D41, `docs/test-results/12a-osal-sockets-sim.md`):
a blocking host `select()` starves lower-priority tasks completely; polling with `vTaskDelay(1)` leaves them
91–98 % at ~1 % host CPU, latency ≤ 1 ms; committed. Phase 2 done 2026-11-13: host sockets with polling
select in the OS layer (simulator only); 04 75/75 with network-api-test 164/164 and select-test 132/132, 06 5/5,
05b 3/3 (`--clean`), 08/09/09b unchanged (no socket code on the Cortex-M3), 07-check-bundle exit 0; closed and
committed (user approved). 12b done 2026-11-13 (D42): ci_lab/to_lab in the simulator mission; 12b script exit 0
(NOOP over UDP, counter_app HK at 1/s with command counter 0 -> 1, events as telemetry, 1.8 % host CPU), 06 5/5,
05b 3/3, 08 unchanged; closed and committed (user approved). 12c done 2026-11-13 (D43): ground station
`tools/ground_station/ground_station.py` (Python stdlib; listen, send, interactive); 12c script exit 0 (bytes
identical to cmdUtil, all counter_app commands and error cases seen in telemetry, 2 s period on the console),
06 5/5; closed and committed (user approved). Roadmap of steps 11-12 complete. Step 13 (user 2026-11-13: option
B, tag `v3-tctm-sim`): 13a done (D44) — the bundle's simulator configuration has sch_lab/ci_lab/to_lab, networking
and the ground station; build-time `/cf` image with tables; 07-check-bundle exit 0 (TC/TM in the clone);
closed and committed (user approved). Next action: 13b — re-export (clean source revision), 07-check-bundle,
then the user pushes with a token (docs/BUNDLE-PUBLISH.md). Known: an incremental build right after a CMake input
change can fail in CMake configure ("Configuring incomplete"); `--clean` avoids it (seen in 11b and 12a).

12a plan (OSAL sockets on the simulator; summary): (1) Phase 1, investigate and stop: a test program on the
FreeRTOS POSIX port (`tests/freertos-posix-socket-block/`) — a high-priority task waits in a host `select()`
while a low-priority task counts; measure starvation, EINTR and timeouts, and the alternative (zero-timeout
`select()` + `vTaskDelay(1)` polling). Reasoned, not yet tested: a task in a blocking host call still counts as
running for FreeRTOS, so lower-priority tasks would not run. (2) Phase 2, expected design: reuse OSAL's portable
`os-impl-bsd-sockets.c` and `os-impl-posix-network.c` unchanged; our own `OS_SelectSingle/Multiple_Impl` that
polls and blocks with `vTaskDelay`; stream records typed RAM file or socket, `OS_Generic*_Impl` dispatching
sockets to host `read`/`write`/`close`; `os-impl-sockets.h`/`os-impl-io.h` in `osal-ext/freertos/inc`; compiled
only for networking on and BSP `freertos-posix-sim` (Cortex-M3 keeps the stubs); networking on in
`config/osal/freertos-posix-sim/osal_config.cmake` (shared by script 04 and the simulator cFE mission); the 5
network entries removed from script 04's `KNOWN_FAILS` (D26 updated). Risks: polling cost (~1000 host selects/s
per waiting socket), SIGPIPE on TCP send, RAM file system I/O through the new dispatch, bundle export (its
simulator config keeps networking off). (3) Verify: 04 exit 0 with no network known failures, 06, 05b,
08/09/09b unchanged (no socket symbols on the Cortex-M3), 07-check-bundle; D41, test results, ARCHITECTURE.

Future work (user decisions 2026-11-13): (1) counter_app CDS kept across a processor reset — needs PSP
processor-reset support (simulator: file-backed reserved memory and re-exec; board: no-init RAM) and a restart
command (step 12); (2) bundle support for build-time table files in `/cf` (generate the `/cf` image at build time
after the tables), so sch_lab can join the bundle.

User decisions: order 11a → 11b → 11c → 11d → 12a → 12b → 12c; **no floating point** (the user will raise it
if needed); **no QEMU runs** in these steps (features tested on the Linux simulator; script 08 still checks the
Cortex-M3 build and RAM budget, since the counter_app source stays common); network: UDP on the simulator only,
the Cortex-M3 flight configuration keeps networking off; ground station: own minimal Python (standard library
only, no install), not NASA's cFS-GroundSystem (PyQt5).

- **11a events and file I/O:** EVS events for start-up, errors and test results (the `counter_app counter: N`
  line stays: script 06 timing check); write, read back, compare, append, list and delete a file on `/ram`, read
  `/cf`; OSAL file calls only, no C stdio.
- **11b sample_lib:** link `sources/cFS/libs/sample_lib` statically, `CFE_LIB` line before the app in the startup
  script, call its function from counter_app.
- **11c commands, housekeeping, table, CDS:** command pipe (NOOP, RESET_COUNTERS, SET_PERIOD, standard command
  codes and length checks); housekeeping telemetry on an HK request sent by `sch_lab` once per second; period from
  a cFE table loaded from `/cf`; counter kept in the CDS across a processor reset. Commands themselves are tested
  over UDP once step 12 exists.
- **11d child task and timer:** worker task sharing data with the main task under an OSAL mutex, plus a binary
  semaphore; an OSAL timer drives the period instead of `OS_TaskDelay`.
- **12a OSAL sockets on the simulator:** first test whether a FreeRTOS POSIX-port task can block in a host
  `select`/`recvfrom` (signals, EINTR, tick patch 0001); then enable OSAL networking for the simulator only,
  reusing OSAL's portable BSD-socket code if blocking is safe, otherwise a non-blocking variant; socket code kept
  separable so the board can use a different TCP/IP stack later; run OSAL's network tests.
- **12b ci_lab + to_lab** in the simulator mission: commands in on UDP 1234, telemetry out on UDP 1235.
- **12c ground station** `tools/ground_station/` (Python 3, stdlib `socket`/`struct`): CCSDS headers matching the
  mission config (checked against cFE's headers and `cmdUtil`); sends the `to_lab` output enable, counter_app and
  ES commands; decodes housekeeping and EVS events; logs to a file; an automated end-to-end test script.

Facts checked 2026-11-12: OSAL network is off in both OSAL configs and the FreeRTOS OS layer has no socket code;
ci_lab/to_lab use OSAL sockets (UDP); OSAL ships `os-impl-bsd-sockets.c`/`os-impl-bsd-select.c`. Later and
separate: networking on the SmartFusion2 board (TCP/IP stack such as lwIP or FreeRTOS+TCP, MSS Ethernet driver
blocked by D7); possibly QEMU's LAN9118 emulation as a middle step (not checked). Risk: Cortex-M3 RAM headroom
is about 24 KB (500,396 of 524,288 bytes); each step re-measures with `scripts/10-memory-report.sh`.

## Known issues on the VirtualBox VM host (recorded 2026-11-06, user decision: keep as known, not investigated)

Host `mpfs-devel`: VirtualBox VM, 2 vCPU, kernel 7.0.0-42, same Ubuntu/gcc/cmake as the Docker host. All scripts
00–08 were re-run there on 2026-11-06. On the Docker host the same scripts pass. Logs (`logs/`) are not in git.

| # | Script / test | Seen | What happens | Assessment |
|---|---|---|---|---|
| K1 | 04 `osal_timer_UT` 05.011/05.012 | 2 of 6 rounds | interval min 466–468 ms / max 530–534 ms vs window 475–525 ms (500 ms ±5 %) | host timing jitter (D15, D21) |
| K2 | 04 `count-sem-test` | 1 of 6 rounds | lowest-priority task's `OS_CountSemTimedWait(1000)` returns -7 (timeout); the test still counts the loop step, so 7 counter assertions follow (8 FAIL) | host timing; not seen elsewhere |
| K3 | 04 `sem-speed-test` | 1 of 3 rounds (2026-11-06, pinned) | main task's 5 s `OS_TaskDelay` overran by ~1 min: both workers finished all 10,000,000 iterations and exited, so both `OS_TaskDelete` return -16 (2 FAIL); teardown not finished within the 90 s limit (exit 137) | the workers did not deadlock; the high-priority sleeper was woken far too late, so FreeRTOS time stopped advancing properly. Reasoned: tick delivery starved by the host. Not proven. Related to D24 (sem-speed hang on the same VM) |
| K4 | 06 counter_app | 1 of 10 runs | drift 13.0 ms over 18 s vs limit 10 ms (all intervals within ±50 ms) | host timing |
| K5 | 05b round 3 (2026-11-06, before the set comparison) | 1 of 6 rounds | SB "4 Thread Bulk Transfer": counts frozen at 162754/162746 CMD, 162730/162722 TLM for >5 min while the test main task kept printing every second; killed at 300 s | **unexplained.** Clock and main task alive, transfer tasks stopped: looks like a lost wake-up / deadlock in the OSAL FreeRTOS layer (queues or `os-impl-sem.c`), not starvation. Not reproduced in the 3 rounds that followed. If seen again: run the image under gdb as its own child on this VM and inspect what the four tasks wait on |

Rule of thumb: on this VM a single failure of K1–K4 is expected and is not a new bug. K5 (and any stall where the
clock still runs) should be treated as a possible real defect.

### K5: deferred (user decision 2026-11-12)

Not being worked on for now; may be picked up later. K5 stays open and unexplained. The investigation plan
drafted 2026-11-06 (never approved or started) is kept here so it is not lost:

- **Facts from `sb_performance_test.c:531-600` and the recorded counts:** `CmdXmit`/`TlmXmit` (prio 150) wait on
  their own counting semaphore with no timeout; `CmdRecv`/`TlmRecv` (prio 100) call `CFE_SB_ReceiveBuffer(..., 5000)`.
  The 5 s receive timeout never fired, and sent − received = 8 (the pipeline depth) in both pairs, so the receivers
  had messages and still did not read them. Both pairs froze together; they share only the SB lock and the scheduler.
- **Leading hypothesis:** receivers blocked on something without a timeout (SB lock or a lock in our OSAL queue
  code) whose holder never runs again or never releases it: (a) our OSAL code (would also hit the Cortex-M3), or
  (b) the FreeRTOS POSIX port / tick patch 0001 (simulator only).
- **Phase 0:** get the VM's `logs/05b-cfe-ft/freertos-round3-console.log` if it still exists; point the stale notes
  (05b re-run section above, "logs to be sent"; `docs/test-results/05b-cfe-functional-tests-sim.md`, "not analysed
  yet") to K5.
- **Phase 1 (desk review):** every blocking path the test uses — OSAL queue get/put and their staging mutexes,
  mutexes, counting semaphores, object-table lock; cFE SB transmit/receive/allocate/release and the SB lock;
  patch 0001 and the port's suspend/resume path.
- **Phase 2 (fast reproducer, test-only):** app `apps/sb_stress/` in a mission `freertos-posix-sim-stress` repeating
  the 4-task pattern with a watchdog (no progress for 10 s → dump FreeRTOS task states via `uxTaskGetSystemState`,
  mutex holders, queue fill levels, and what each task blocks on via a compile-time `OS_FREERTOS_DEBUG_BLOCKING`
  hook, off by default); script `scripts/k5-sb-stress-sim.sh`, optionally under gdb as its own child on the VM.
- **Phase 3:** stop and report; fix our OSAL code, or fix/record the port/patch 0001 cause, or record the
  no-reproduction run time.
- **Phase 4:** ~1 h stress run on each host, 05b with `ROUNDS=10` on both, regression 04/05/06/07, docs.
- **Risks:** may only reproduce on the VM (0 stalls in ~30 rounds on the Docker host); instrumentation may hide the
  race; the reproducer may not hit the same path (fallback: 05b in a long loop on the VM).

## Environment facts that matter
- Current host (since 2026-11-04): Docker container, 2 vCPU Xeon Platinum 8370C, Ubuntu 24.04.5. No effective
  capabilities: gdb cannot attach to a running process at all (`tools/debug/ptrace-launch.c` does not help here);
  gdb can still run a program as its own child. Core dumps go to the host's apport pipe and are not reachable.
- The 2-vCPU VirtualBox VM (`mpfs-devel`) is still in use alongside the Docker host: `usleep(1000)` ≈ 2.7 ms; busy
  threads could starve others for seconds (D14, D15, D21). Suspect host starvation first for any simulator hang or
  timing drift there (known issues K1–K4 above). On this VM gdb can run a program as its own child, and
  `tools/debug/ptrace-launch.c` allows attaching later (ptrace_scope=1). The ARM toolchain is installed there too
  (2026-11-06, same versions as the Docker host); script 08 passes.
- Git: public repo https://github.com/coskuncelik/m2s-cfs, branch `main` (D23). Commit/push only with approval.
