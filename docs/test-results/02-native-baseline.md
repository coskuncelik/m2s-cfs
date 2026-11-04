# Test result — 02 native-POSIX cFS baseline

- Date: 2026-11-03 15:17 +03
- Host: Ubuntu 24.04.5 LTS x86_64; gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0; cmake version 3.28.3; GNU Make 4.3
- Sources: cFS `c62b5b9c507af4972aad09ee9f3443fa16a9fc85` (cFE `fc8c48a`, OSAL `a77b5bd`, PSP `ae0c0a7`), unmodified
- Configuration: `SIMULATION=native`, `MISSIONCONFIG=sample`, `MISSION_DEFS=sources/cFS/cfe/cmake/sample_defs` (read-only)
- Command: `./scripts/02-build-native-baseline.sh` (smoke run: `core-cpu1 -R PO` for 10 s)

## Results

| Check | Result |
|---|---|
| Clean build (`--clean`) | PASS — exit 0, 0 compiler warnings |
| Incremental rebuild | PASS — exit 0 |
| `--no-run` (build only) | PASS — exit 0 |
| Smoke run: POWER ON reset, OPERATIONAL state, SAMPLE_APP initialized | PASS — two consecutive runs |
| Negative test: `RUN_SECONDS=1` (too short to reach OPERATIONAL) | Correctly FAILED with exit 1 |
| `sources/cFS` working tree unchanged after build | PASS (`git status --porcelain --ignore-submodules=none` empty) |

## Observations

- Without `-R PO`, a later run can start as a PROCESSOR reset, because the pc-linux PSP keeps SysV shared
  memory segments (CDS 128 KiB, reset area, user reserved 1 MiB) after SIGINT. Fixed by forcing `-R PO`.
- The PSP cannot set real-time scheduling as non-root (`setschedparam ... Operation not permitted`).
  This is harmless; tasks run under the default policy.
- CI_LAB listens on UDP port 1234 while running.
- The cFE timestamps start at 1980 (no time source set) — expected for the baseline.

## Smoke-run log excerpt

```text
OS_Posix_TaskAPI_Impl_Init():405:Could not setschedparam in main thread: Operation not permitted (1)
CFE_PSP: Starting the cFE with a POWER ON reset.
1980-001-01:05:53.09718 CFE_ES_Main: CFE_ES_Main in EARLY_INIT state
1980-001-01:05:53.09797 CFE_ES_Main: CFE_ES_Main entering CORE_STARTUP state
1980-012-14:03:20.50907 CFE_ES_Main: CFE_ES_Main entering CORE_READY state
EVS Port1 1980-012-14:03:20.56436 66/1/SAMPLE_APP 1: Sample App Initialized.Sample App Development Build equuleus-rc1+dev54 (Codename Equuleus), Last 
1980-012-14:03:20.61595 CFE_ES_Main: CFE_ES_Main entering APPS_INIT state
1980-012-14:03:20.61607 CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state
```
