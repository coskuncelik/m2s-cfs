# Test result — 06 counter_app on the Linux simulator

- Date: 2026-11-05
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs, Intel Xeon Platinum 8370C), gcc 13.3.0, cmake 3.28.3
- Sources (unmodified in `sources/`): cFS `c62b5b9` (cFE equuleus-rc1+dev247, OSAL `a77b5bd`, PSP equuleus-rc1+dev73),
  FreeRTOS-Kernel `8a59a4b` (V11.3.2)
- Project code: `apps/counter_app/` (new), mission config `config/cfe/freertos-posix-sim/` (static app, startup
  script line), `scripts/06-run-counter-app-sim.sh` (new), `scripts/lib/cfe-sim.sh` (build steps shared with
  script 05). Decision: D28 (updates D6).
- Command: `./scripts/06-run-counter-app-sim.sh --clean` (RUNS=5, RUN_SECONDS=20, TOLERANCE_MS=50, DRIFT_MS=10) — exit 0

## Results

| Check | Result |
|---|---|
| Clean build + install; counter_app with `-Wall -Wextra -Werror` | PASS |
| No FreeRTOS heap or dynamic FreeRTOS object creation in `core-cpu1` | PASS |
| `malloc`/`free` referenced only by the host POSIX port (2 of 152 target objects) | PASS |
| 5 runs × 20 s: still running at the end; ES loads `COUNTER_APP` and reaches OPERATIONAL; "counter_app initialized" exactly once; counter 1..19 without gap or repeat; every interval 1 s ± 50 ms; first-to-last drift within 10 ms; no error line | PASS — 5/5 |
| Regression: `05-build-cfe-freertos-sim.sh` (5/5), `ROUNDS=3 04-build-osal-freertos-sim.sh --clean` (75/75) | PASS |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |

Per run (host clock, `$EPOCHREALTIME` stamped as each line arrives):

| Run | Counter lines | Max interval deviation | Drift over 18 s |
|---|---|---|---|
| 1 | 19 | 0.833 ms | +2 µs |
| 2 | 19 | 0.103 ms | +1 µs |
| 3 | 19 | 0.259 ms | +6 µs |
| 4 | 19 | 0.031 ms | −26 µs |
| 5 | 19 | 0.185 ms | −2 µs |

19 lines in 20 s: cFE needs about 0.6 s to start the app, and the first count follows 1 s later.

## Output (run 1, abridged; host stamps removed)

```
1980-012-14:03:20.51000 CFE_ES_ParseFileEntry: Loading file: /cf/counter_app.so, APP: COUNTER_APP
counter_app initialized
1980-012-14:03:20.61199 CFE_ES_Main: CFE_ES_Main entering APPS_INIT state
1980-012-14:03:20.61199 CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state
counter_app counter: 1
counter_app counter: 2
counter_app counter: 3
...
counter_app counter: 19
```

`/cf/counter_app.so` is only the name cFE derives from the script's file-name field; nothing is read from it. OSAL's
static loader finds module `COUNTER_APP` in the generated table
`{ .Name = "COUNTER_APP_Main", .Address = &COUNTER_APP_Main, .Module = "COUNTER_APP" }` — the first use of the static
loader path in this project (osal_loader_UT does not cover it, D26).

## Found during the step

- The first version waited with a plain `OS_TaskDelay(1000)`. Measured: about 1.001 s per line (18 intervals took
  18.018 s), because our `OS_TaskDelay` adds one tick so a delay is never shorter than asked. The app now waits for
  fixed 1 s deadlines from `OS_GetLocalTime` (still `OS_TaskDelay`): 18 intervals take 18.000 s ± 26 µs.
- The script's checks were tested against edited logs: a 1 ms/s drift, a missing count, a second "initialized"
  line, an error line and an early exit each make the run fail.

## Notes and open points

- Facts: the same app source uses only cFE/OSAL APIs (`CFE_EVS_Register`, `CFE_ES_RunLoop`, `CFE_ES_ExitApp`,
  `OS_printf`, `OS_TaskDelay`, `OS_GetLocalTime`); no C library I/O, no platform code, no dynamic memory.
- Not shown yet: the same source on the SmartFusion2 board (needs the board BSP/PSP and D7 sources).
- The host-clock check measures the whole path (app → OSAL console task → BSP `write(1)` → pipe), so it also covers
  console latency on the simulator.
