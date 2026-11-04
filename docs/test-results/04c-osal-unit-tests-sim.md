# Test result — 04c full OSAL functional test set on the Linux simulator

- Date: 2026-11-04
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs, Intel Xeon Platinum 8370C), kernel
  6.8.0-1064-azure, glibc 2.39, gcc 13.3.0, cmake 3.28.3
- Sources (unmodified in `sources/`): OSAL `a77b5bd75d611670a6292f0a68922274d3dbf33f`,
  FreeRTOS-Kernel `8a59a4b19bf72977819feddef87d41163ced7004` (V11.3.2)
- Changes in this step: `scripts/04-build-osal-freertos-sim.sh` (25 programs, `KNOWN_FAILS` list),
  `osal-ext/freertos/src/os-impl-filesys.c` (minimum entry count), `osal-ext/freertos/inc/os-freertos.h`
  (simulator pool 1 MiB). Decision: D26.
- Command: `ROUNDS=3 ./scripts/04-build-osal-freertos-sim.sh --clean` — exit 0, 7 min 17 s

## Results

| Check | Result |
|---|---|
| Clean build of OSAL + 25 test programs (project code `-Wall -Wextra -Werror`) | PASS |
| RAM file system core test (ASan + UBSan) | PASS — 109/109 |
| No FreeRTOS heap or dynamic object creation in any of the 25 binaries | PASS |
| 25 programs × 3 rounds; failures only the exact listed ones (D22, D26) | PASS — 75/75 |
| Same summary line for every program in all 3 rounds | yes |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |

Programs new in 04c (identical in all 3 rounds). PASS/FAIL/MIR/N/A are OSAL's own counts; TSF/TTF are setup
and teardown failures counted inside FAIL by the script.

| Program | Total | Pass | Tolerated failures | MIR | N/A | N/A / MIR reasons |
|---|---|---|---|---|---|---|
| osal_core_UT | 228 | 216 | 0 | 8 | 4 | MIR: console output for manual inspection. N/A: select ×2, `OS_HeapGetInfo`, count-semaphore "initial count too high" |
| osal_file_UT | 166 | 165 | 0 | 0 | 1 | `OS_ShellOutputToFile` (no shell) |
| osal_filesys_UT | 53 | 53 | 0 | 0 | 0 | — |
| osal_timer_UT | 39 | 36 | 0 | 3 | 0 | MIR: timer asked for a 5 µs interval; 1 ms tick gives intervals 0–2000 µs, 100 callbacks in 34 ms |
| osal_loader_UT | 22 | 13 | 7 (D26) | 0 | 2 | symbol lookup / symbol table dump not implemented |
| osal_network_UT | 2 | 0 | 0 | 1 | 1 | network not included |
| condvar-test | 1 | 0 | 0 | 1 | 0 | condition variables not implemented, tests skipped |
| select-test | 19 | 16 | 0 | 0 | 3 | network not implemented |
| shell-test | 4 | 3 | 0 | 0 | 1 | shell not implemented |
| symbol-api-test | 7 | 3 | 0 | 0 | 4 | symbol table dump ×3, symbol lookup |
| network-api-test | 20 | 12 | 5 (D26) | 0 | 3 | INET6, network not implemented ×2 |

The 14 programs from 04a/04b gave the same results as before, including the one D22 assertion in osal-core-test.

## Found and fixed

- osal_file_UT creates a 32 KiB RAM disk and opens `OS_MAX_NUM_OPEN_FILES` (50) files on it. With one entry per
  2 KiB the disk had 16 entries, so 21 setup steps failed. A volume now gets at least `OS_MAX_NUM_OPEN_FILES` + 8
  entries, capped at one per 256 bytes. After the fix: 165/165 pass, 1 N/A.
- symbol-api-test creates a 1 MiB RAM disk without caller memory; the 256 KiB pool refused it. The simulator pool
  is now 1 MiB (D26). After that, its symbol functions report N/A as intended.

## Tolerated (D26)

Exactly these assertions, each matched by test name and text in `KNOWN_FAILS`; a run with one missing, one extra
or a different failure is an error (checked against hand-edited logs):

- network-api-test.c lines 99, 103, 108, 113, 119: socket address functions return `OS_ERR_NOT_IMPLEMENTED`
  (network off by configuration) and the test does not allow it.
- ut_osloader_module_test.c lines 128, 141, 156, 159, 195, 232 and ut_osloader_symtable_test.c line 158: module
  loads from files fail with `OS_ERR_NOT_IMPLEMENTED` (dynamic loader off); line 159 is the unload of the module
  that was never loaded.

## Dynamic memory evidence (host build)

`nm -u` over every object: only the POSIX port (`port.c`, `wait_for_event.c`) and the OSAL test harness
(`utlist.c`, `utstubs.c`) reference `malloc`/`free` — the same four objects as in 04a and 04b. osal_loader_UT's
build also makes host shared libraries `MODULE0..n` (`ut_module.c`); they are never loaded and do not exist on
the board.

## Notes and open points

- Facts: all 25 OSAL functional programs run; the only failures are D22 (simulator stack) and D26 (features off).
- Not covered: the static module loader cFE will use (osal_loader_UT tests file loading only) — cFE step.
- Not run: `src/unit-test-coverage` (white-box tests of OSAL's own implementations, out of scope).
- Open for the board: the RAM-disk pool size (1 MiB is the simulator value).
