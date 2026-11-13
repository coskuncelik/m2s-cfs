# Test result — 11a counter_app: events and file I/O (simulator)

- Date: 2026-11-13
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Decision: D37. Scope (user, 2026-11-13): tested on the Linux simulator; Cortex-M3 build and RAM budget only
  (script 08), no QEMU runs.
- Code: `apps/counter_app/fsw/src/counter_app.c`, new `apps/counter_app/fsw/src/counter_app_events.h`;
  checks in `scripts/lib/counter-check.sh` (shared by scripts 06, 09 and 07-check-bundle).

## What counter_app does now

- EVS events (port 1 prints them on the console): 1 initialized, 2 file test passed, 3 file test error (error
  type), 4 startup script read from `/cf`. The `OS_printf` lines (`counter_app initialized`, counter) are unchanged.
- Start-up file test, once, before the 1 s schedule starts, OSAL file API only, static buffers: (1) `OS_mkdir`
  `/ram/counter`; (2) create and write 256 pattern bytes; (3) `OS_stat` size 256; (4) read back and compare;
  (5) `OS_lseek` to the end, append 256, size 512; (6) directory listing = exactly `test.dat`; (7) `OS_rename`,
  `OS_remove`, `OS_rmdir`, file gone; (8) read `/cf/cfe_es_startup.scr`, must contain `COUNTER_APP`. Success: events
  4 and 2 and `counter_app file test: PASS`. First failing step: event 3 and `counter_app file test: FAIL step N`;
  the counter keeps running.
- New expected lines in every counter_app run check: events 1, 4, 2 and `counter_app file test: PASS`.

## Results (verified)

| Check | Result |
|---|---|
| `./scripts/06-run-counter-app-sim.sh --clean`: build (`-Wall -Wextra -Werror`), 5 × 20 s, all expected lines, no error line, timing (max deviation 1.535 ms, drift ≤ 103 us) | PASS 5/5 (exit 0) |
| Console of a run: `COUNTER_APP 1: counter_app initialized`; `COUNTER_APP 4: Read /cf/cfe_es_startup.scr: 422 bytes, names COUNTER_APP`; `COUNTER_APP 2: File test passed on /ram/counter: wrote 256, read back, appended to 512 bytes, listed, renamed, removed`; `counter_app file test: PASS` | as expected |
| Failure path (temporary, uncommitted change: test directory `/nonexistent/counter`; `RUNS=1`): event `COUNTER_APP 3: File test error at step 1 (mkdir /nonexistent/counter), status -108`, line `counter_app file test: FAIL step 1`; the run check reports 3 missing lines and 2 error lines | detected (exit 1) |
| Source restored afterwards (`cmp` with the saved copy), then `06` again | PASS 5/5 (exit 0) |
| `./scripts/08-build-cfe-arm-cm3.sh --clean` (Cortex-M3 flight build): no heap, no allocator references; RAM budget .data + .bss + .main_stack = 501,940 bytes ≤ 524,288 (95 %, +1,544 bytes); text 217,267 (+4,452) | PASS (exit 0) |
| `sources/cFS` unchanged | PASS |

The first failure-path run used the earlier event text "File test step 1 (...) went wrong", which the error
pattern does not match (only the FAIL line counted: 1 error line); the text was changed to "File test error at
step ..." and the failure-path run repeated (2 error lines).

## Not verified

- The QEMU runs (scripts 09 and 07-check-bundle use the same checks and would now expect the new lines): not
  run in this step by user decision.
- The bundle includes `apps/counter_app`; the next export will make a local, untagged bundle commit with this
  change (D35a). Nothing is exported or pushed in this step.
