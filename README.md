# m2s-cfs

NASA cFS/cFE on FreeRTOS Kernel V11.3.2, targeting:

- a Linux simulator (FreeRTOS POSIX port), and
- the Microchip SmartFusion2 M2S150-ADV-DEV-KIT (ARM Cortex-M3).

## Quick start

```bash
./scripts/00-basic-setup.sh          # pin and verify all source submodules (needs git and network access)
./scripts/01-check-prerequisites.sh  # read-only host tool check; prints the apt command for anything missing
./scripts/02-build-native-baseline.sh # build stock cFS (native POSIX OSAL) and smoke-run it for 10 s
./scripts/03-build-freertos-posix-smoke.sh # FreeRTOS V11.3.2 POSIX port smoke test (no cFE), 10 timed runs
./scripts/04-build-osal-freertos-sim.sh --clean # OSAL on FreeRTOS (simulator BSP): 25 OSAL functional test programs x3 + RAM file system core test
./scripts/05-build-cfe-freertos-sim.sh --clean  # stock cFE core on FreeRTOS with the project PSP, 5 runs of 20 s
./scripts/06-run-counter-app-sim.sh --clean     # counter_app on the simulator: start-up message + 1 Hz counter, 5 runs of 20 s
```

## Documentation

- `CLAUDE.md` — working rules
- `docs/ARCHITECTURE.md` — how cFE and OSAL run on FreeRTOS (layers, design)
- `docs/VERSIONS.md` — exact source revisions
- `docs/REFERENCES.md` — reference projects and findings
- `docs/STATUS.md` — current progress and next actions
- `docs/DECISIONS.md` — recorded decisions
- `docs/test-results/` — recorded test results
- `patches/` — project patches to pinned sources (applied to copies at build time)

Status: the stock native-POSIX cFS baseline builds and runs (`docs/test-results/02-native-baseline.md`).
The pinned FreeRTOS kernel runs on the Linux host (POSIX port, static allocation, patch 0001), without cFE
(`docs/test-results/03-freertos-posix-smoke.md`). OSAL runs on FreeRTOS on the simulator with a project-owned,
static-memory OS layer: 12 OSAL functional tests pass (`docs/test-results/04a-osal-freertos-sim.md`), and with
the static RAM file system the OSAL file tests pass too (`docs/test-results/04b-ramfs-sim.md`). All 25 OSAL
functional test programs run, failing only on documented simulator limits and features left out
(`docs/test-results/04c-osal-unit-tests-sim.md`). Stock cFE core starts and runs on FreeRTOS on the simulator
with a project PSP, from static memory only (`docs/test-results/05-cfe-freertos-sim.md`), and `counter_app` prints
its start-up message and a counter every second (`docs/test-results/06-counter-app-sim.md`). The SmartFusion2
target is not done yet.
