# Test result — 11d counter_app: worker task and timer

- Date: 2026-11-13
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Decision: D40.
- Code: `apps/counter_app/fsw/src/counter_app.c` (timer, worker task, `COUNTER_APP_SetPeriod`, main loop),
  `counter_app_cmds.c` (SET_PERIOD and table update through `COUNTER_APP_SetPeriod`; housekeeping reads the
  shared fields under the mutex), `counter_app.h`, `counter_app_events.h` (18, 19); `scripts/lib/counter-check.sh`
  expects event 18.

## Design

An OSAL timer (`OS_TimerCreate`, its own time base; interval = period) gives the binary semaphore
`COUNTER_TICK` from its callback. The child task `COUNTER_WORKER` (`CFE_ES_CreateChildTask`, stack 4096 from the
static pool, cFE priority 49) takes it, increments the counter under the mutex `COUNTER_DATA` and prints the
counter line. The main task waits on the command pipe (1 s timeout) for commands and housekeeping requests and
runs `CFE_TBL_Manage`; SET_PERIOD and a table update store the period under the mutex and call `OS_TimerSet`.
The deadline loop of steps 06–11c is gone. Mutex, semaphore, pipe, table, worker or timer failures stop the app
(error events 17/19).

## Results (verified)

| Check | Result |
|---|---|
| `06 --clean` (simulator): event `COUNTER_APP 18: Worker task and timer started: period 1000 ms, timer accuracy 1000 us`; first counter line 1.0 s later; max interval deviation 1.422 ms, drift ≤ 1.1 ms | PASS 5/5 (exit 0) |
| Period from the timer (temporary, uncommitted: table default 2000 ms; `RUNS=1`): event 18 reports 2000 ms, counter lines 2000.0 / 1999.9 / 2000.8 / 1999.2 ms apart; run rejected (1 s timing, 9 lines, missing table and worker lines) | detected (exit 1) |
| Header restored (`cmp`), `06` again | PASS 5/5 |
| `08 --clean` (Cortex-M3): RAM .data + .bss + .main_stack = 504,764 bytes ≤ 524,288 (96 %, +16); text 233,291 | PASS (exit 0) |
| `09 --clean` (Cortex-M3 under QEMU) | PASS 5/5 (exit 0) |
| `10-memory-report.sh --clean`: COUNTER_WORKER 684/4096 (83 % free), COUNTER_APP 1536/4096 (62 %), COUNTER_TIMER time base helper 360/2048 (82 %); tightest still CFE_ES 2712/4096 (34 %); OSAL tasks 11/24, mutexes 17/20, binary semaphores 4/20, time bases 2/5, timers 3/10, queues 7/16 | PASS (exit 0) |
| `07-check-bundle.sh`: same tree, both configurations built in a fresh clone, simulator 5/5, QEMU 5/5 | PASS (exit 0) |
| Published bundle unchanged (`git ls-remote`: main = `ed41597`) | PASS |

## Not verified

- SET_PERIOD reprogramming the timer at run time and a table update at run time: no command source yet (step
  12c).
- Mutexes are at 17 of 20 on the Cortex-M3 flight configuration (OSAL limit unchanged in step 10 because of this).
