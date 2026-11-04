# Test result — 03 FreeRTOS V11.3.2 POSIX-port smoke test

- Date: 2026-11-03 16:33 +03
- Host: Ubuntu 24.04.5 LTS x86_64 in an Oracle VirtualBox VM (2 vCPUs), kernel 7.0.0-42-generic
  (`PREEMPT_LAZY`, HZ=1000), glibc 2.39, gcc 13.3.0, cmake 3.28.3
- Sources: FreeRTOS-Kernel `8a59a4b19bf72977819feddef87d41163ced7004` (V11.3.2), unmodified in `sources/`
- Project inputs: `config/freertos/posix/FreeRTOSConfig.h`, `config/freertos/posix/freertos_posix_hooks.c`,
  `patches/freertos-kernel/0001-posix-port-tick-catch-up.patch`, `tests/freertos-posix-smoke/`
- Command: `./scripts/03-build-freertos-posix-smoke.sh --clean` (RUNS=10)
- No cFE, no OSAL. Test: a 1 s `xTaskDelayUntil` ticker (priority 2) and a 500 ms worker (priority 1),
  all objects static, stdout serialized with a static FreeRTOS mutex; ticker ends the scheduler after 5 periods.

## Results

| Check | Result |
|---|---|
| Patch 0001 applies cleanly to the pinned `port.c` (dry run) | PASS |
| Clean build, `-Wall -Wextra` (test code `-Werror`) | PASS — exit 0, 0 warnings |
| No FreeRTOS heap: link map and `nm` have no `heap_*.c`, `pvPortMalloc`, `vPortFree` | PASS |
| Negative test: `-DSMOKE_FORCE_HEAP_USE=ON` must fail to link | PASS — `undefined reference to 'pvPortMalloc'` |
| Smoke runs (each period ±150 ms, total 5000 ±200 ms) | PASS — 10/10; totals 4999–5007 ms; worst period 1026 ms |
| Incremental re-run (`RUNS=3`, no `--clean`) | PASS — exit 0 |
| Unknown argument | Correctly FAILED with exit 1 |
| `sources/FreeRTOS-Kernel` working tree clean after build | PASS |

## Investigation that led to D14–D16

Before the fixes the test hung: `ticker: 1`, `worker: heartbeat 3`, then nothing until killed.

| Measurement (all on this VM) | Result |
|---|---|
| 1000 × `usleep(1000)`, plain program | 2637–2852 ms |
| Minimal FreeRTOS program, stock port, pinned to CPU 0 | 2501 ticks took 6636 ms |
| Same, unpinned / pinned to CPU 1 | stalled after ~1.2 s / no output in 20 s |
| 3000 × 1 ms absolute sleeps with 0 / 1 / 2 busy-spinning threads (no FreeRTOS) | 3315 / 7763 / 39377 ms; worst single sleep 28 / 68 / 14896 ms |
| Stock port + sleeping idle hook (3 runs) | no stall; 2501 ticks in 5531–5961 ms |
| Patched port + sleeping idle hook (3 runs) | 2509–2511 ticks in 2506–2511 ms |
| Patched port + idle hook, smoke test with the old ±50 ms limit | 4/5 PASS, 1 FAIL (total 5144 ms) |

gdb traces showed the stall: IDLE was the current task and busy-spinning, the other tasks were correctly
parked in `event_wait`, and the port's tick thread did not run for seconds.

## Notes and open points

- Facts: the hang came from (1) tick drift in the stock port's `usleep` loop and (2) the VM starving a
  sleeping thread while others spin. Point (2) was reproduced without FreeRTOS.
- Reasoned conclusion: the idle hook removes the spinner the port adds, but busy cFS tasks on the simulator
  could still hit the VM starvation. Watch for this in later phases.
- Not tested: a non-VM host. The ±150/±200 ms tolerances are set for this VM.
- The POSIX port itself uses glibc `malloc` (`port.c`, `wait_for_event.c`) and glibc pthread stacks. That is
  host-side only and is not evidence about the embedded target (D4).
