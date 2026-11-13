# Test result — 12c ground station on the simulator

- Date: 2026-11-13
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs), Python 3.14.2
- Decision: D43.
- New: `tools/ground_station/ground_station.py` (+ `README.md`): Python 3 standard library; `listen`, `send`,
  `interactive`; commands tlm-enable, noop, reset, set-period, es-noop, raw; decodes counter_app housekeeping and
  EVS event messages; log file. `scripts/12c-run-ground-station-sim.sh` (automated end-to-end test). No flight
  code changed.

## Results (verified)

| Check | Result |
|---|---|
| Byte check: the same four commands from cmdUtil and from the ground station to a local check port — noop `1890c000000100b6`, set-period 2000 `1890c00000050267d0070000`, tlm-enable, es-noop `1806c00000010020` | identical |
| `./scripts/12c-run-ground-station-sim.sh` (third run, see below): 9 commands sent with `ground_station.py send`, 15 telemetry checks — HK after tlm-enable (counters 0); NOOP event 10 and command counter 1; SET_PERIOD 2000 event 12, HK period 2000, command counter 2; SET_PERIOD 100 rejected (error event 13, error counter 1, period still 2000); unknown code 9 (error event 14, error counter 2); wrong length (error event 15 "length 12, expected 8", error counter 3); reset (event 11, counters 0); period back to 1000 (command counter 1); ES NOOP event (CFE_ES 3) | PASS (exit 0) |
| Console: counter lines 2000 ms and 2000 ms apart while the period was 2000 ms; no error line besides counter_app events 13, 14, 15 | PASS |
| Interactive mode (manual smoke test with commands piped in): tlm-enable, noop (event, command counter 0 → 1), an unknown word (usage text), quit; exit 0 | works |
| `06` (simulator, flight code unchanged) | PASS 5/5 (below) |

## Findings (test script, fixed)

- Run 1: all commands sent, then the script hung while stopping its processes: background jobs of a
  non-interactive shell start with SIGINT ignored, so `kill -INT` did not stop the ground station listener
  (core-cpu1 installs its own SIGINT handler and stopped). The listener and the console stamper now get SIGTERM.
- Run 2: all 15 telemetry checks passed; the console interval check failed on a parsing error in the script (the
  measured interval was 2000 ms) and had only one interval to check; fixed, and the 2000 ms phase lengthened
  from 7 to 9 s (two intervals).
