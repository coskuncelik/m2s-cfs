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
```

## Documentation

- `CLAUDE.md` — working rules
- `docs/VERSIONS.md` — exact source revisions
- `docs/REFERENCES.md` — reference projects and findings
- `docs/STATUS.md` — current progress and next actions
- `docs/DECISIONS.md` — recorded decisions
- `docs/test-results/` — recorded test results
- `patches/` — project patches to pinned sources (applied to copies at build time)

Status: the stock native-POSIX cFS baseline builds and runs (`docs/test-results/02-native-baseline.md`).
The pinned FreeRTOS kernel runs on the Linux host (POSIX port, static allocation, patch 0001), without cFE
(`docs/test-results/03-freertos-posix-smoke.md`). The OSAL FreeRTOS layer has not started yet.
