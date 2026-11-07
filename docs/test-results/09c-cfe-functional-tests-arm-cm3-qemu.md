# Test result — 09c cFE functional tests on an emulated Cortex-M3 (QEMU mps2-an385)

- Date: 2026-11-07
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs), cmake 3.28.3
- Tools (see `docs/VERSIONS.md`): `arm-none-eabi-gcc` 13.2.1, binutils 2.42, newlib-nano 4.4.0, QEMU 8.2.2
- Sources (unmodified in `sources/`): cFS `c62b5b9` (cFE equuleus-rc1+dev247, OSAL `a77b5bd`, PSP
  equuleus-rc1+dev73, sample_app, sample_lib), FreeRTOS-Kernel `8a59a4b` (V11.3.2, `GCC/ARM_CM3`)
- Mission `freertos-arm-cm3-qemu-ft` (overlay on `config/cfe/freertos-arm-cm3`: the simulator's functional-test
  mission with `cpu1_SYSTEM freertos-arm-cm3`, board `qemu-mps2-an385`, test-only allocator); decision D34
- Command: `./scripts/09c-run-cfe-functional-tests-arm-cm3-qemu.sh --clean` (ROUNDS=3, RUN_TIMEOUT=600; QEMU default
  timing, no `-icount`). Exit 0.

## Results

| Check | Result |
|---|---|
| Copied mission files identical to the simulator's functional-test mission (`targets.cmake` except `cpu1_SYSTEM`) | PASS |
| Native reference (stock pc-linux, same apps) | PASS — 2521 assertions, 2044 PASS, 0 FAIL, 13 MIR, 41 s |
| Cortex-M3 build (two passes, generated tables in the `/cf` image); image ELF32 ARMv7-M Thumb-2, entry `Reset_Handler`, no `.got` outside `.data`, link map | PASS |
| Test image: test allocator only (`test_heap.c`), no allocator traps, no newlib allocator, no FreeRTOS heap or dynamic objects | PASS |
| 3 rounds under QEMU: log rebuilt with the reported size; no `M2S TRAP/FAULT/ASSERT/PANIC/STOP` line; every assertion equal to native (status + `file:line`, in order; as a set inside the two multi-task SB tests), **no allowed difference**; identical lists in all rounds | PASS — 3/3 |
| D29's `es_task_test.c:108` (fails on the simulator by design) | PASS on the Cortex-M3 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |
| Regression (board linker scripts, `.got` check, comparison code moved to `scripts/lib/cfe-ft.sh`): `08 --clean` exit 0 (new `.got` check passes), `09 --clean` 5/5, `05b --clean` 3/3 (simulator, via the shared library), `09b --clean` 75/75 | PASS |

| Run | Total | PASS | FAIL | MIR | Time |
|---|---|---|---|---|---|
| Native (stock) | 2521 | 2044 | 0 | 13 | 41 s |
| Cortex-M3 round 1–3 | 2521 | 2044 | 0 | 13 | 191–203 s |

Image (`arm-none-eabi-size`): text 394885, data 1392, bss 9689276 bytes (16 MiB PSRAM). SB bulk rates (MIR,
informational): single thread 11,277–11,607 msg/s, 2 threads 10,686–11,110, 4 threads 7,379–7,717.

All of cFE (ES, EVS, SB, TBL, TIME, FS, CDS, message and resource-ID APIs) gives exactly the native result on the
Cortex-M3, including the stack-address check that the simulator cannot pass.

## Problems found and fixed on the way

1. **GOT not initialised (real defect, both board linker scripts).** The first run locked up after TBL init
   (`qemu: fatal: Lockup: can't escalate 3 to HardFault`); QEMU's exception log showed the SysTick and HardFault
   vectors read as 0. A gdb watchpoint on the SysTick vector caught `memset(0x0, 0, ...)` from
   `UtAssert_BeginTest()` (`CFE_Assert_LibInit`): `ut_assert` is built with `POSITION_INDEPENDENT_CODE`
   (`sources/cFS/osal/ut_assert/CMakeLists.txt:62`) and reaches `UT_SegmentCounters` through the GOT, but
   `board.ld` had no rule for `.got`/`.got.plt`; the linker placed them after `_edata`, outside the range
   `Reset_Handler` copies, so every GOT entry read 0 and the `memset` cleared the vector table (RAM at address 0
   on this board). Fixed: `*(.got .got.*)` and `*(.got.plt)` inside `.data` in both `boards/*/board.ld`;
   `check_got` (scripts 08, 09, 09c) fails if a `.got` output section exists outside `.data`. The flight images
   of steps 08/09 had no `.got` (no position-independent code), so they were not affected.
2. **QEMU timing mode.** With `-icount` (as in step 09) one round took about 25 min (tests about 6 min, log dump
   over the polled UART about 19 min, SB 2,368 msg/s); without it about 2 min (dump 6.8 s, SB 10,654 msg/s), with
   the same totals. 09c uses QEMU's default timing (user decision, D34): these tests check results, not timing
   precision, which script 09 still covers with `-icount`.

## Not covered

- QEMU, not the SmartFusion2 (see the step 09 page). In QEMU's default timing mode SysTick follows the host
  clock; a heavily loaded host could make a time-sensitive cFE assertion fail by chance (not seen in 3 rounds).
- The test allocator's high-water mark is printed only when a program exits; a cFE run does not exit (QEMU is
  stopped after the log dump), so it is not reported here (09b reports it for the OSAL tests).
- A write through a NULL pointer silently overwrites the vector table on this board (RAM at 0) instead of
  trapping; the Cortex-M3 MPU could make it fault (possible hardening for the board step).
