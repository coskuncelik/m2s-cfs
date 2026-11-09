# Test result — 07b bundle with the simulator and QEMU Cortex-M3 configurations, history kept

- Date: 2026-11-09
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs), gcc 13.3.0, cmake 3.28.3, GNU tar 1.35,
  arm-none-eabi-gcc 13.2.1 (newlib-nano 4.4.0), QEMU 8.2.2
- Sources (unmodified in `sources/`): cFS `c62b5b9`, FreeRTOS-Kernel `8a59a4b` (V11.3.2)
- Decision: D35 (extends D30).
- Command: `./scripts/07-check-bundle.sh` (runs the export twice; RUNS=5, RUN_SECONDS=20, TOLERANCE_MS=50,
  DRIFT_MS=10; QEMU with `-icount` and a 3 s warm-up launch). Exit 0.

## Results

| Check | Result |
|---|---|
| Export starts from the published bundle (clone of github.com/coskuncelik/cfs-m2s-freertos, `main` = `d48850f`) and commits on top of it; two exports give the same tree (`c29bd808eb77`); the commit's parent is the published `main` | PASS |
| 3149 upstream files byte-identical; only the listed root changes; no file outside upstream + added + generated | PASS |
| No private references in the committed files or in the commit message; no private files | PASS |
| Fresh clone: `make MISSIONCONFIG=freertos_sim O=build-sim prep && make O=build-sim && make O=build-sim install` | PASS |
| Fresh clone: the same with `MISSIONCONFIG=freertos_qemu O=build-qemu` (cross build, cFE strict flags) | PASS |
| Clone working tree clean after both builds (`cf.tar`, `build-*/` ignored) | PASS |
| `freertos_sim`: no FreeRTOS heap; `malloc`/`free` only in the host POSIX port (2 of 152 objects); counter_app 5 × 20 s (script 06 checks) | PASS — 5/5 |
| `freertos_qemu`: ELF32 ARMv7-M Thumb-2, entry `Reset_Handler`, vectors, no `.got` outside `.data`, link map, no allocator or heap linked, no object referencing `malloc`/`free` (0 of 152) | PASS |
| `freertos_qemu`: counter_app 5 × 20 s under QEMU (`-icount`), script 09 checks (OPERATIONAL, counter 1..19, 1 s ± 50 ms, drift ≤ 10 ms, no `M2S` line) | PASS — 5/5 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |
| Regression in this repository (shared `counter-check.sh` changed): `06` 5/5, `09` 5/5 | PASS |

Per run:

| Run | Simulator: max deviation / drift | QEMU: max deviation / drift |
|---|---|---|
| 1 | 0.104 ms / +96 µs | 3.740 ms / +1851 µs |
| 2 | 1.290 ms / −24 µs | 2.898 ms / −551 µs |
| 3 | 0.044 ms / +12 µs | 2.716 ms / +387 µs |
| 4 | 0.075 ms / −67 µs | 2.842 ms / −978 µs |
| 5 | 0.196 ms / 0 µs | 2.178 ms / −826 µs |

Bundle (local, not pushed): `main` = `ed41597` "Add the QEMU Cortex-M3 configuration; mission configs freertos_sim
and freertos_qemu" on top of `d48850f`; tags `v1-sim` (`d48850f`) and `v2-sim-qemu` (`ed41597`); 3266 files. QEMU
image: text 213475, data 1296, bss 7568796 bytes.

## Problems found and fixed on the way

- `make prep` failed for `freertos_sim`: "No implementation for perfids.h found". cFE looks for
  `<MISSIONCONFIG>_perfids.h` and has no default; the old bundle worked only because its configuration was
  named `sample` (stock `sample_perfids.h`). The export now adds `<name>_perfids.h` (a copy of the stock file) to
  each configuration.
- The Cortex-M3 files carried private references (step numbers, decision numbers, repository paths) and
  repository-relative CMake paths (kernel helper, FreeRTOS config); fixed by exact replacements in the export
  (39 in total) and bundle versions of the configuration files in `export/freertos_qemu_defs`.
- First full check: QEMU run 1 printed its first line about 5 s after launch (later runs: 0.2 s), so only
  14 counter lines fit in the 20 s run. Cause (reasoned, not proven): QEMU's cold start right after the two
  builds on a busy 2-vCPU host. Fixed with one untimed 3 s warm-up launch before the counted QEMU runs
  (`RUN_WARMUP_SECONDS` in `scripts/lib/counter-check.sh`, off by default); timing limits unchanged.

## Not covered

- Published by the user on 2026-11-09 (normal push with a fine-grained token): `main` = `ed41597`, tags `v1-sim`
  = `d48850f`, `v2-sim-qemu` = `ed41597`, verified with `git ls-remote`. Not checked: a teammate's host.
- Teammates with an existing clone must delete their old `build/` folder (the README says so).
- The board configuration `freertos_m2s` comes with the board step.
