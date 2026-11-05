# Test result — 07 shareable bundle `cfs-m2s-freertos`

- Date: 2026-11-05
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs, Intel Xeon Platinum 8370C), gcc 13.3.0, cmake 3.28.3,
  GNU tar 1.35
- Sources (unmodified in `sources/`): cFS `c62b5b9` (cFE equuleus-rc1+dev247, OSAL `a77b5bd`, PSP equuleus-rc1+dev73),
  FreeRTOS-Kernel `8a59a4b` (V11.3.2)
- Project code:
  - new: `scripts/07-export-bundle.sh`, `scripts/07-check-bundle.sh`, `scripts/lib/counter-check.sh` (counter
    checks moved out of script 06), and `export/` (bundle-only files: `README.md`, `PORTING.md`, `NOTICE`,
    `gitignore`, `sample_defs/`, `mission_build_custom.append.cmake`);
  - changed: `cmake/freertos-posix-kernel.cmake` (kernel, config and patch locations can be set by the caller).
- Decision: D30.
- Command: `./scripts/07-check-bundle.sh` (it runs the export twice; RUNS=5, RUN_SECONDS=20, TOLERANCE_MS=50,
  DRIFT_MS=10). Exit 0.

## Results

| Check | Result |
|---|---|
| Two exports give the same git tree | PASS |
| 3149 upstream files (cFS with its submodules, FreeRTOS-Kernel) byte-identical; only the listed root changes (`.gitignore` replaced, `README.md` → `README-cFS.md`, `.github/` and `.gitmodules` left out); no file outside upstream + added + generated | PASS |
| No private references (decision numbers, private docs, steps, scripts, host paths) and no private files in the committed bundle | PASS |
| Fresh `git clone` builds with the stock `make prep && make && make install` (cFE strict flags, `-Werror`); the build leaves the clone's working tree clean | PASS |
| Clone's `core-cpu1`: no FreeRTOS heap or dynamic FreeRTOS object creation; `malloc`/`free` referenced only by the host POSIX port (2 of 152 target objects) | PASS |
| Clone's `core-cpu1`, 5 runs × 20 s: script 06's checks (ES loads `COUNTER_APP`, OPERATIONAL, "counter_app initialized" once, counter 1..19 without gap, every interval 1 s ± 50 ms, drift within 10 ms, no error line) | PASS — 5/5 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |
| Regression in this repository (kernel helper and script 06 changed): `06 --clean` 5/5, `05` 5/5, `03 --clean` 10/10, `04 --clean` 75/75 | PASS |

Per run (host clock):

| Run | Counter lines | Max interval deviation | Drift over 18 s |
|---|---|---|---|
| 1 | 19 | 0.167 ms | −11 µs |
| 2 | 19 | 0.994 ms | +9 µs |
| 3 | 19 | 0.285 ms | −2 µs |
| 4 | 19 | 0.519 ms | −12 µs |
| 5 | 19 | 0.673 ms | −1 µs |

(Final run, on the final state of the files. An earlier full run, before the README and commit-date fixes, also
passed 5/5.)

Bundle: 3219 files, 45 MB checked out (21 MB git data), one commit (tree `dd9a3351f44f`). `core-cpu1`: text 498645, data 5776, bss 7632720 bytes.

## Problems found and fixed on the way

- `make install` failed: `tables/staging` not found. cFE's `prepare()` deletes and recreates `build/tables`
  after `mission_build_custom.cmake` runs, so that directory is now created at the end of the configure step
  (`cmake_language(DEFER)`). This raises the minimum CMake version for the bundle to 3.19. In this repository's
  build, the scripts create the directory instead.
- The leak check also matched the bundle's own path `freertos-posix-sim/config/freertos_posix_hooks.c`. The
  pattern now only matches this repository's `config/<dir>/` paths.
- The README's `nm` command also counted OSAL's test library `ut_assert`, which is not linked into `core-cpu1`.
  It now excludes `ut_assert` and the stubs and prints the object names.

## Not covered

- The bundle is checked on this host only. A teammate's host may have another gcc, cmake or tar.
- `export/sample_defs` is kept equivalent to `config/cfe/freertos-posix-sim` by hand; script 07-check-bundle
  catches a drift that breaks the build or the run, not every difference.
- The bundle is not published yet (separate approval).
