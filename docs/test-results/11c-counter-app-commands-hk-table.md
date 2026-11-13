# Test result — 11c counter_app: commands, housekeeping, parameter table; sch_lab

- Date: 2026-11-13
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Decision: D39. User decisions: CDS left as future work; sch_lab scope "your recommendation"; then option A
  (sch_lab only in the project missions, counter_app's table loaded from memory) after the bundle's `/cf` limit
  was found; bundle support for build-time table files (option B) is future work.

## What was added

- counter_app (`apps/counter_app/`): public headers `fsw/inc/counter_app_msgids.h` (topics 0x90 command, 0x91
  housekeeping request, 0x90 housekeeping telemetry = MIDs 0x1890, 0x1891, 0x0890), `counter_app_msg.h` (NOOP 0,
  RESET_COUNTERS 1, SET_PERIOD 2 with `PeriodMs`; housekeeping payload: counter, period, sample_lib status,
  command and error counters, file test result), `counter_app_tbl.h` (table "Params": default period and
  SET_PERIOD limits, defaults 1000 / 500..5000 ms, absolute bounds 100..10000 ms); `fsw/src/counter_app_cmds.c`
  (pipe, subscriptions, command handling with length and code checks, housekeeping on request, table register,
  load from memory with `CFE_TBL_SRC_ADDRESS`, validation, `CFE_TBL_Manage` each period); main loop reads the
  pipe with a timeout until each deadline (period from the table or SET_PERIOD). Events 7–17.
- sch_lab (NASA, unchanged) in the simulator and Cortex-M3 missions: static app list, startup line (priority
  80), `/cf/sch_lab_table.tbl` (`cf_files.txt`, two-pass build), mission table override
  `config/cfe/<mission>/tables/sch_lab_table.c` (one entry: counter_app's housekeeping request at 1 Hz, TickRate
  10); counter_app's `fsw/inc` on the include path through `global_build_options.cmake`.
- `scripts/lib/cfe-sim.sh`: the mission config assembly now copies sub-folders (`tables/`).
- `scripts/lib/counter-check.sh`: expects the "Table loaded" event everywhere; "SCH Lab Initialized." and
  "First housekeeping packet sent" only in the project missions (`07-check-bundle.sh` sets
  `COUNTER_CHECK_BUNDLE=1`).
- Bundle (`export/`): no change of its mission configs; it gets the new counter_app source.

## Results (verified)

| Check | Result |
|---|---|
| `06 --clean` (simulator): two build passes (sch_lab table into `/cf`); events "Table loaded: period 1000 ms, limits 500..5000 ms", "SCH Lab Initialized.", "First housekeeping packet sent (MID 0x0890)" about 1.9 s after start; no error line; timing unchanged (max deviation 1.061 ms) | PASS 5/5 (exit 0) |
| Invalid table (temporary, uncommitted: default 50 ms below the 500 ms minimum; `RUNS=1`): event 9 "Table validation error: default 50 ms, limits 500..5000 ms", load error, app exits; run rejected (2 missing lines, 2 error lines, 0 counter lines) | detected (exit 1) |
| Header restored (`cmp`), status casts fixed (below), `06` again | PASS 5/5 |
| `08 --clean` (Cortex-M3): RAM .data + .bss + .main_stack = 504,748 bytes ≤ 524,288 (96 %, +2,784); text 232,491 (+14,584); no heap | PASS (exit 0) |
| `09 --clean` (Cortex-M3 under QEMU, run because script 10 compares against its image): RAM 504,740 bytes; counter_app with sch_lab and housekeeping lines | PASS 5/5 (exit 0) |
| `10-memory-report.sh --clean`: COUNTER_APP 1680/4096 (59 % free), SCH_LAB_APP 1176/4096 (71 % free), CFE_ES 2712/4096 (34 % free, tightest); SB pool 1,408/65,536; TBL pool 36,944/65,536 (56.4 %: two 16 KiB load buffers + two tables); OSAL tasks 10/24, queues 7/16, mutexes 16/20, timers 2/10 | PASS (exit 0) |
| `07-check-bundle.sh` first run: exports, builds, simulator 5/5; QEMU 4/5 (run 1: counter 5 at +82.2 ms, counter 6 at −80.6 ms, drift 1.1 ms) | FAIL (exit 1) |
| `07-check-bundle.sh` rerun: same tree, both configurations built in a fresh clone, simulator 5/5, QEMU 5/5 (with "Table loaded") | PASS (exit 0) |
| `05b` (cFE functional tests on the simulator; the functional-test mission inherits the base mission folder and uses the changed config assembly): 3/3 rounds, same as native except the 1 known D29 difference | PASS (exit 0) |
| Published bundle unchanged (`git ls-remote`: main = `ed41597`) | PASS |

## Findings

- The first invalid-table run printed the load status as `0xFFFFFFFFFFFFFFFF` on the 64-bit simulator (a
  negative `int32` cast straight to `unsigned long`); all status prints now cast through `uint32`.
- The TBL pool is above the "≤ 50 % of the pool" figure of step 10 (56.4 %): fixed load buffers at start-up plus
  4 KB of tables; about 28 KB remain for tables.
- The QEMU timing failure has the host-side signature seen before (one line late, the next as early, no drift).

## Not verified

- `09c` (cFE functional tests on the Cortex-M3 under QEMU) was not re-run; it uses the same config assembly as
  05b, which passed.

- Commands (NOOP, RESET_COUNTERS, SET_PERIOD, length and code errors) and the housekeeping packet contents: no
  command source and no telemetry receiver yet; built and reviewed only, tested in step 12c over UDP (roadmap).
- A table update at run time (`CFE_TBL_Manage` returning updated): needs a table load command (step 12).
