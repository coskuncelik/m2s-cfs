# Test result — 12a OSAL sockets on the simulator

- Decision: D41. Plan approved by the user 2026-11-13 (phase 1 first, then stop and report).

## Phase 1: a FreeRTOS POSIX-port task waiting in a host socket call (2026-11-13)

- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Test: `tests/freertos-posix-socket-block/main.c` (pinned FreeRTOS V11.3.2 POSIX port, patch 0001, project
  `FreeRTOSConfig.h`, static allocation; no cFE, no OSAL). Command: `./scripts/12a-posix-socket-block.sh --clean`
  (RUNS=3), exit 0; full output `logs/12a-posix-socket-block-runs.log` (logs are not in git).
- Tasks: "low" (priority 1) counts in a busy loop; "waiter" (priority 3) waits 3 s on a UDP socket bound to
  127.0.0.1. Baseline: low task count per second while the waiter is in `vTaskDelay` (436–460 million/s).
  Scenarios: A one blocking `select()` with the remaining time as timeout, repeated after EINTR; B zero-timeout
  `select()` then `vTaskDelay(1)`; C = A and D = B with a datagram sent after 1 s by a forked child (send time in
  the datagram, host `CLOCK_MONOTONIC`).

| Scenario (3 runs) | Low task CPU (% of baseline) | `select()` calls / EINTR in 3 s | Waiter CPU | Receive latency |
|---|---|---|---|---|
| A blocking, no data | **0 %, 0 %, 0 %** | 2966–2984 / 2965–2984 | 10.6–10.9 ms/s | — |
| B polling, no data | 93.0 %, 94.8 %, 98.5 % | 2783–2828 / 0–2 | 9.8–10.1 ms/s | — |
| C blocking, data at 1 s | **0 %, 0 %, 0 %** | 2957–2985 / 2955–2983 | 10.5–11.1 ms/s | 37, 40, 350 us |
| D polling, data at 1 s | 91.4 %, 95.5 %, 90.9 % | 2726–2822 / 1–2 | 9.9–10.2 ms/s | 250, 986, 227 us |

### Findings (verified by the measurements)

- A task waiting in a host `select()` starves every lower-priority FreeRTOS task completely for the whole wait
  (0 % in all 6 blocking cases): the host thread sleeps, but FreeRTOS still sees the task as running.
- The host call is interrupted by the tick signal about once per millisecond (EINTR ≈ number of calls, ~1000/s),
  so a single blocking call returns after about 1 ms; code that does not retry on EINTR would see an early error.
- Polling (zero timeout, then `vTaskDelay(1)`) lets lower-priority tasks run (91–98 % of baseline) at a cost of
  about 10 ms of host CPU per second (1 % of a core) per waiting task, with ~940 polls per second.
- Receive latency with polling: 0.23–0.99 ms (at most one tick); with blocking: 0.04–0.35 ms.

### Conclusion (reasoned)

OSAL's portable `os-impl-bsd-select.c` waits in a blocking host `select()` and is therefore not usable as is in
the simulator. With NASA's sample startup priorities and our OSAL-to-FreeRTOS mapping (0–255 onto 30 levels),
`ci_lab` (OSAL 60, level 23) waiting in it would starve the lower levels: ES and TBL (68, 70: level 22), `to_lab`
(70: 22) and `sch_lab` (80: 21) — so housekeeping requests and telemetry would stop. TIME, EVS and SB (60–64)
share level 23 and still run by time slicing; counter_app and its worker (50, 49: level 25) and the TIME tone and
1 Hz tasks (25: level 27) are higher and not affected.

This is specific to the FreeRTOS POSIX port (the scheduler does not see a task blocked in a host system call).
It does not happen in native Linux cFS (every task is a Linux thread) nor on a FreeRTOS target with a FreeRTOS
TCP/IP stack (a socket wait blocks on a FreeRTOS semaphore or queue).
Phase 2 uses the planned design: OSAL's portable socket code with our own `OS_SelectSingle/Multiple_Impl` that
polls with a zero timeout and blocks the task in FreeRTOS (`vTaskDelay`, one tick) between polls.

## Phase 2: OSAL sockets on the simulator (2026-11-13)

Implementation (D41): OSAL's portable `os-impl-bsd-sockets.c` and `os-impl-posix-network.c` (unchanged) plus
`osal-ext/freertos/src/os-impl-host-sockets.c` (socket handle table, polling `OS_SelectSingle_Impl` /
`OS_SelectMultiple_Impl`: zero-timeout host `select()`, `vTaskDelay(1)` between polls; socket stream
read/write/close with `MSG_DONTWAIT`, `MSG_NOSIGNAL` on send, EINTR retried); headers `os-impl-io.h`,
`os-impl-sockets.h`, `os-impl-network.h` in `osal-ext/freertos/inc`; `OS_Generic{Read,Write,Seek,Close}_Impl` in
`os-impl-filesys.c` hand socket streams to it; stream init in `os-impl-common.c`. Compiled only with
`OSAL_CONFIG_INCLUDE_NETWORK` and BSP `freertos-posix-sim` (`OS_FREERTOS_HOST_SOCKETS`; the network sources with
`_XOPEN_SOURCE=600` like OSAL's generic-linux BSP); otherwise the no-network/no-sockets/no-select stubs, and
networking on another BSP stops the build. Networking on in `config/osal/freertos-posix-sim/osal_config.cmake`
(script 04 and the simulator cFE missions); Cortex-M3 config unchanged. `scripts/lib/osal-tests.sh`: the 5
network entries moved to `OSAL_KNOWN_FAILS_NO_NETWORK`, used only by script 09b.

| Check | Result |
|---|---|
| `04 --clean` (all OSAL tests on the simulator, 3 rounds): `select-test` 132/132 PASS (before: 19 assertions, 3 N/A "network not implemented"), `network-api-test` 164/164 PASS (before: 20, 5 tolerated failures D26), `osal_network_UT` 4 (3 PASS, 1 MIR host id, as on OSAL's POSIX port; before 2); no known failure left for networking | PASS 75/75 (exit 0); repeated after the `_XOPEN_SOURCE` fix: 75/75 (exit 0) |
| `06 --clean` (simulator cFE + counter_app): first run failed to build — `gethostname`/`gethostid` implicitly declared under cFE's `-std=c99 -pedantic -Werror`; fixed with `_XOPEN_SOURCE=600` on the three network sources | FAIL (exit 2), then PASS 5/5 (exit 0) |
| Simulator image: `OS_SocketOpen_Impl`, `OS_FreeRTOS_HostSocketRead`, host `socket`, `select` linked | as expected |
| `05b` (cFE functional tests, simulator): first run (before the fix) failed to build as 06; an incremental rerun failed in CMake ("Configuring incomplete", two parallel make targets reconfiguring the same sub-build after a CMake input changed); `05b --clean` | PASS 3/3 (exit 0), same as native except the 1 known D29 difference |
| `08 --clean` (Cortex-M3): RAM 504,764 bytes (unchanged); no `OS_SocketOpen_Impl`, `socket` or `select` in the image | PASS (exit 0) |
| `09 --clean` (Cortex-M3 under QEMU) | PASS 5/5 (exit 0) |
| `09b --clean` (OSAL tests on the Cortex-M3, networking off, the 5 network entries from `OSAL_KNOWN_FAILS_NO_NETWORK`) | PASS 75/75 (exit 0) |
| `07-check-bundle.sh` (bundle's simulator config keeps networking off): run before and after the CMake fix | PASS (exit 0) both; published bundle unchanged (`ed41597`) |

### Findings

- The incremental-build CMake failure ("Configuring incomplete" with interleaved configure output) is the second
  occurrence (first in 11b, then unexplained): it follows a change of a CMake input and goes away with
  `--clean`. Reasoned: concurrent re-configuration of the cFE per-target sub-build by parallel make targets.
- Polling cost under cFE is not measured yet: no cFE app waits on a socket until `ci_lab` (12b).
