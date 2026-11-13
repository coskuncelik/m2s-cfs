# Test result — 11b counter_app uses sample_lib

- Date: 2026-11-13
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Decision: D38. Bundle check with its QEMU runs approved as an exception (user: option A).
- Code: `apps/counter_app/CMakeLists.txt` (`add_cfe_app_dependency(counter_app sample_lib)`),
  `apps/counter_app/fsw/src/counter_app.c`, `counter_app_events.h` (IDs 5, 6); `sample_lib` added to
  `targets.cmake` (static app and symbol lists) and a `CFE_LIB` line before counter_app in `cpu1_cfe_es_startup.scr`
  of `config/cfe/freertos-posix-sim`, `config/cfe/freertos-arm-cm3`, `export/freertos_sim_defs`,
  `export/freertos_qemu_defs`; checks in `scripts/lib/counter-check.sh`. `sources/` unchanged.

## Results (verified)

| Check | Result |
|---|---|
| `06 --clean`: ES loads `SAMPLE_LIB` ("Loading shared library: /cf/sample_lib.so") before `COUNTER_APP`; `SAMPLE Lib Initialized.Sample Lib Development Build equuleus-rc1+dev2 ...`; `SAMPLE_LIB_Function called, buffer='SAMPLE DATA'`; event `COUNTER_APP 5: sample_lib call returned 0x00000000`; `counter_app sample_lib call: PASS`; no error line; timing unchanged | PASS 5/5 (exit 0) |
| Missing library (temporary, uncommitted: `CFE_LIB` line removed from the simulator startup script, `RUNS=1`): run rejected, "missing line: SAMPLE Lib Initialized." and "missing line: SAMPLE_LIB_Function called, buffer='SAMPLE DATA'" | detected (exit 1) |
| Startup script restored (`cmp` with the saved copy), `06` again | PASS 5/5 |
| `08 --clean` (Cortex-M3 flight build): RAM .data + .bss + .main_stack = 501,964 bytes (95 %, +24); text 217,907 (+640); no heap | PASS (exit 0) |
| `07-check-bundle.sh`, first run: export failed — "private references left in the bundle": a comment added in 11a named `scripts/lib/counter-check.sh` (see below) | FAIL (exit 1) |
| After rewording the comments: export OK (local bundle commit, 8 files, untagged, not pushed); `07-check-bundle.sh` second run: both configurations built, simulator 5/5, QEMU 4/5 | FAIL (exit 1) |
| `07-check-bundle.sh` third run: same tree `e3d9cff58f8b`, both configurations built in a fresh clone, simulator 5/5, QEMU (Cortex-M3) 5/5 with `SAMPLE Lib Initialized.`, `counter_app file test: PASS`, `counter_app sample_lib call: PASS` | PASS (exit 0) |
| Published bundle unchanged (`git ls-remote`: main = `ed41597`) | PASS |

## Findings

- Without its `CFE_LIB` init, `SAMPLE_LIB_Function()` still returns `CFE_SUCCESS` (it prints an empty buffer), so
  counter_app reports PASS; only the expected-line check (`buffer='SAMPLE DATA'`, `SAMPLE Lib Initialized.`)
  catches a missing library. The app's error branch (event 6, `sample_lib call: FAIL`) cannot be exercised
  without changing `sources/`; reviewed only.
- 11a's comments in `counter_app.c` referred to project steps, decisions and `scripts/lib/counter-check.sh`; the
  bundle export (which includes `apps/counter_app`) rejects such private references. 11a did not run the export,
  so this was not seen then. The comments (11a and 11b) were reworded without project references.
- QEMU run 1 of the second bundle check had 17 counter lines (18 needed). counter_app's own start-up was the
  same as in the passing runs (initialized 0.58 s and counter 1 at 1.58 s after the first console line); the
  first console line came about 1.5 s later than usual, i.e. QEMU started late on the host. Reasoned: host
  launch latency; the third run passed 5/5.
- One `06` run (incremental, right after the comment change) stopped in CMake configure ("configure_file: No
  such file or directory", exit 2). Not reproduced: `06 --clean` and an incremental `06` afterwards both passed
  5/5. Unexplained; disk space was fine (11 GB free).
