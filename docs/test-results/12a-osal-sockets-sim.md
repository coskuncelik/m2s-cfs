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
