# Test result — 09b OSAL functional tests on an emulated Cortex-M3 (QEMU mps2-an385)

- Date: 2026-11-06
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs), cmake 3.28.3
- Tools (see `docs/VERSIONS.md`): `arm-none-eabi-gcc` 13.2.1, binutils 2.42, newlib-nano 4.4.0, QEMU 8.2.2
- Sources (unmodified in `sources/`): cFS `c62b5b9` (OSAL `a77b5bd`), FreeRTOS-Kernel `8a59a4b` (V11.3.2, `GCC/ARM_CM3`)
- Build: OSAL on its own (`config/osal/freertos-arm-cm3/toolchain.cmake`), OS layer `osal-ext/freertos`, BSP
  `osal-ext/freertos-arm-cm3`, board `boards/qemu-mps2-an385`; TEST images with the test-only allocator
  `boards/common-cm3/test_heap.c` (D33)
- Command: `./scripts/09b-run-osal-tests-arm-cm3-qemu.sh --clean` (ROUNDS=3; QEMU `-icount shift=auto,sleep=on,align=on
  -semihosting-config enable=on,target=native`, one QEMU guest per test program). Exit 0.

## Results

| Check | Result |
|---|---|
| OSAL and 25 test programs build for the Cortex-M3 (project code `-Wall -Wextra -Werror`) | PASS |
| Every test image: no FreeRTOS heap or dynamic FreeRTOS object creation; test allocator linked, allocator traps not; no newlib allocator object in its link map | PASS — 25/25 |
| 25 tests × 3 rounds: QEMU exit status 0 and no FAIL/TSF/TTF, or exit status 1 with exactly D26's configuration failures; no `M2S TRAP/FAULT/ASSERT/PANIC/STOP` line | PASS — 75/75 |
| osal-core-test 02.003 (D22, fails on the simulator by design) | PASS on the Cortex-M3: 622/622 in all 3 rounds |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |
| Regression (shared board code, BSP exit path, CMake flags and script libraries changed): `08 --clean` exit 0 (allocator entry points only from `alloc_traps.c`, `_sbrk` discarded), `09 --clean` 5/5, `04 --clean` 75/75 (test list now from `scripts/lib/osal-tests.sh`), `06 --clean` 5/5, `05` 5/5 | PASS |

Compared with the simulator (script 04, round 1 summaries): every test has the same TOTAL/PASS/FAIL/MIR, except
osal-core-test (simulator 621 PASS + 1 FAIL, D22; Cortex-M3 622 PASS). The allowed failures on the Cortex-M3 are
only D26's 12 (5 in network-api-test, 7 in osal_loader_UT: network and module loading are off). Timing tests
(timer-test, time-base-api-test, osal_timer_UT, sem-speed-test, the timeout tests) pass in every round.

Test allocator high-water marks: 184–4496 bytes of the 1 MiB pool (osal_core_UT the largest). A first one-round
run (`ROUNDS=1`) also passed 25/25 before the full run.

Before this step the Cortex-M3 image had run only cFE + counter_app (step 09). Now the whole OSAL functional
test set runs on the Cortex-M3: real per-task stacks with FreeRTOS stack-overflow checking on, 32-bit code, the
`ARM_CM3` port's context switching, the static RAM file system, timers and time bases on SysTick.

## Problems found and fixed on the way

- The flight allocator-origin check (`scripts/lib/cm3-image.sh`, scripts 08 and 09) still expected the traps in
  `syscalls.c` after they moved to `alloc_traps.c`; the first regression run of 08 and 09 failed on that check
  only (the link maps showed the symbols from `alloc_traps.c`). The check now requires `alloc_traps.c` for the
  allocator entry points and `syscalls.c` for `_sbrk`.

## Not covered

- QEMU, not the SmartFusion2 (see the step 09 page).
- cFE's functional tests on the Cortex-M3 (planned step 09c).
- The test allocator is for test images only; its presence there says nothing about flight images, which keep
  the traps (scripts 08, 09).
