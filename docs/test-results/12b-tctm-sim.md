# Test result — 12b commands and telemetry over UDP on the simulator (ci_lab, to_lab)

- Date: 2026-11-13
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Decision: D42.
- Changes (simulator mission `config/cfe/freertos-posix-sim` only): `ci_lab` (priority 60) and `to_lab` (70) in
  the static app list and startup script; `tables/to_lab_sub.c` (mission override: counter_app HK 0x0890,
  ci_lab and to_lab HK, EVS long and short event messages); `tables/sch_lab_table.c` adds ci_lab READ_UPLINK at
  10 Hz and the ci_lab/to_lab HK requests at 1 Hz; `/cf/to_lab_sub.tbl`. New `scripts/12b-run-tctm-sim.sh` and
  `scripts/lib/udp-capture.py` (Python 3 standard library, host side). Cortex-M3 mission and bundle unchanged.

## Results (verified)

| Check | Result |
|---|---|
| `06 --clean` with ci_lab and to_lab: "CI_LAB listening on UDP port: 1234", "CI Lab Initialized", "TO Lab Initialized", no error line; timing unchanged | PASS 5/5 (exit 0) |
| `./scripts/12b-run-tctm-sim.sh`: cFS OPERATIONAL; 4 s with no command: 0 telemetry packets; `cmdUtil --pktid=0x1880 --cmdcode=6 --string=16:127.0.0.1` (to_lab OUTPUT_ENABLE); 3 s later `cmdUtil --pktid=0x1890 --cmdcode=0` (counter_app NOOP) | PASS (exit 0), second run (first run below) |
| Telemetry over UDP 127.0.0.1:1235: event "TO_LAB_APP 3: TO telemetry output enabled for IP 127.0.0.1"; counter_app HK (MID 0x0890, 32 bytes, telemetry header 16 bytes) at 1.00/s with counter 4, 5, 6 …, period 1000, sample_lib status 0, file test 0; event "COUNTER_APP 10: NOOP command received" about 0.5 s after the command; command counter 0 before, 1 from the next HK packet on | as expected |
| Console: NOOP event, no error line | PASS |
| Simulator host CPU over the 12 s run: 0.20 s and 0.21 s (1.7 % and 1.8 % of one core) — includes ci_lab polling the socket at 10 Hz and the 12a polling select | measured |
| `08 --clean` (Cortex-M3): RAM 504,764 bytes, unchanged | PASS (exit 0) |
| `05b --clean` (cFE functional tests, simulator; inherits the base mission folder) | PASS 3/3 (exit 0) |
| `sources/` unchanged | PASS |

## Findings

- First run of script 12b: every check passed ("=== 12b-run-tctm-sim OK"), but the script did not exit: its exit
  handler used a bare `wait`, which also waits for the `tee` of the script's own log (a process substitution
  that ends only when the script exits). Stopped by hand; the handler now waits only for the simulator and the
  capture process. Second run exit 0.
- These are the first runtime tests of counter_app's command path (11c): NOOP reaches the app through ci_lab, the
  command counter and the NOOP event behave as designed.
