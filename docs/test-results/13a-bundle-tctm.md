# Test result — 13a commands and telemetry in the bundle (ready for v3-tctm-sim)

- Date: 2026-11-13
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Decision: D44. User decisions 2026-11-13: option B (commands and telemetry into the bundle before
  republishing), tag `v3-tctm-sim`.

## Changes

- `export/mission_build_custom.append.cmake`: build-time `/cf` image — target `m2s-cf-image` packs the startup
  script and every `tables/staging/cpu1/cf/*.tbl` into `cf.tar` (deterministic tar, copy-if-different) after
  cFE's `tabletool-execute`; `<system>-all` and `<system>-install` depend on it (added with `cmake_language(DEFER)`,
  because those targets are created later). The configure-time image (startup script only) stays.
- `export/freertos_sim_defs/`: networking on; `sch_lab`, `ci_lab`, `to_lab` in `targets.cmake` and the startup
  script; `tables/sch_lab_table.c`, `tables/to_lab_sub.c` (team-facing copies of the project's simulator tables);
  `arch_build_custom.cmake` puts counter_app's `fsw/inc` on the include path.
- Bundle `ground_station/` = `tools/ground_station/ground_station.py` + `export/ground_station_README.md`
  (bundle paths); the ground station's docstring and help text no longer name the project.
- `export/README.md`, `export/PORTING.md`: v3 content (counter_app features, commands and telemetry, ground
  station, sockets on the simulator, build-time `/cf` image).
- `scripts/07-export-bundle.sh`: `_defs` folders copied with sub-folders; private-reference check over all files
  of the `_defs` folders (it only looked at the top level); `ground_station/` added; defaults `BUNDLE_TAG=v3-tctm-sim`
  and the v3 title and release notes.
- `scripts/07-check-bundle.sh`: `ground_station/` in the expected files and the reference check; per
  configuration: `freertos_sim` expects the sch_lab/housekeeping lines, checks `/cf` (startup script and both
  tables) and runs a TC/TM test with the clone's own ground station; `freertos_qemu` checks `/cf` = startup
  script only and keeps `COUNTER_CHECK_BUNDLE=1`.

## Results (verified)

| Check | Result |
|---|---|
| `07-export-bundle.sh`: first run failed on bash syntax (an apostrophe in the `${BUNDLE_BODY:-...}` default), second run on a private reference ("m2s-cfs" in the ground station's help text); fixed at the source | exit 2, exit 1, then exit 0: commit `ce21376` on the published `ed41597`, 30 files, tags `v1-sim`, `v3-tctm-sim` (local, not pushed) |
| `07-check-bundle.sh`: two exports same tree; upstream files identical; no private references; fresh clone builds `freertos_sim` and `freertos_qemu` with one `prep`/`make`/`make install` each | PASS (exit 0) |
| `freertos_sim` in the clone: `/cf` after one make = `cfe_es_startup.scr sch_lab_table.tbl to_lab_sub.tbl`; malloc/free only in the host POSIX port (2 of 165 objects); counter_app with sch_lab and housekeeping lines | 5/5 |
| TC/TM in the clone with the bundle's ground station: tlm-enable, noop, es-noop → housekeeping with command counter 1, NOOP event, ES NOOP event | PASS |
| `freertos_qemu` in the clone: `/cf` = `cfe_es_startup.scr`; image checks, no allocator or heap; counter_app | 5/5 |
| Incremental rebuild in the clone: one subscription removed from `tables/to_lab_sub.c`, one `make` → the embedded `to_lab_sub.tbl` changes (md5 `2cc85031…` → `eca1f9e0…`) and equals the installed one; source restored → original table again | PASS |
| `make` with no change: `cf.tar` and the embedded `.inc` keep their times; `core-cpu1` is relinked anyway because stock cFE regenerates `cfe_build_env_table.c` (build time) on every make | as stock cFE |
| Project scripts: `06` 5/5, `12c` exit 0 (bytes equal to cmdUtil, all checks) | PASS |
| Published bundle unchanged (`ed41597`) | PASS |

## 13b: publication (2026-11-13)

Re-exported after committing 13a (PORTING.md: source revision `f2a45d4`, not dirty): bundle commit `0bf16e7` on
the published `ed41597`, 30 files, tag `v3-tctm-sim`; `07-check-bundle.sh` on this export exit 0 (same tree twice,
3149 upstream files identical, no private references, both configurations built in a fresh clone, `/cf` with both
tables, simulator 5/5, TC/TM with the bundle's ground station, QEMU 5/5). Pushed by the user (normal push,
fine-grained token). `git ls-remote https://github.com/coskuncelik/cfs-m2s-freertos.git`: `refs/heads/main` and
`refs/tags/v3-tctm-sim` = `0bf16e7e7264bc549eff0c94cc937a4005f3a0be` (= local HEAD and tag), `v1-sim` = `d48850f`,
`v2-sim-qemu` = `ed41597`.
