# Test result — 10 memory sizing of the Cortex-M3 flight image

- Date: 2026-11-12
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs)
- Tools (see `docs/VERSIONS.md`): `arm-none-eabi-gcc` 13.2.1, binutils 2.42, QEMU 8.2.2, gdb-multiarch 15.1 (Python)
- Image: mission `freertos-arm-cm3-qemu` (flight configuration `config/cfe/freertos-arm-cm3`, board
  `qemu-mps2-an385`), rebuilt with `-g` in `build/cfe-freertos-arm-cm3-qemu-mem`; data and bss checked equal to
  script 09's image (1296 / 7568796 bytes).
- Decision: D36. Phase 1 changes no configuration.
- Commands: `./scripts/10-memory-report.sh` (RUN_SECONDS=20), exit 0, report `logs/10-memory-report.txt`;
  `./scripts/10-memory-report.sh --ft` (09c's functional-test mission, probed after all tests and the log dump),
  exit 0 (about 3.5 min), report `logs/10-memory-report-ft.txt`. Logs are not in git. Both run twice; the second
  pair after limiting the queue/pipe lists to objects that still exist; the figures were the same.

## How it measures

- Static RAM: link-map input sections in RAM grouped by owning library/object, plus `nm` symbol sizes.
- Run-time use: counter_app runs 20 s under QEMU (same options as script 09) with QEMU's gdb stub; gdb-multiarch
  connects, halts the guest and runs `scripts/lib/mem-probe-gdb.py` (reads memory only):
  - stacks: FreeRTOS fills each new task stack with `0xA5` (`configCHECK_FOR_STACK_OVERFLOW 2`); used = bytes
    below the first untouched byte. Owner = task on a FreeRTOS list whose `pxStack` is that stack;
  - main stack (MSP): counted from zero-filled RAM, approximate;
  - cFE ES pools (SB, TBL): `TailPosition` = high-water; SB's own peak counters; OSAL objects in use.

## Results (verified)

Static RAM 7,570,017 bytes (data + bss + 4 KiB main stack):

| Owner | Bytes | Main items |
|---|---|---|
| PSP | 4,194,620 | `CFE_PSP_FreeRTOS_ReservedMem` 4 MiB, of which 3,406,336 laid out: volatile disk 2 MiB, user reserved 1 MiB, CDS 128 KiB, reset area 129,400 |
| OSAL FreeRTOS layer | 2,137,500 | RAM-disk pool 1 MiB, queue table 539,648 (64 × 8 KiB), stack pools 458,752, timebase table 42,700, `/cf` root 32 KiB |
| cFE TBL | 577,488 | buffer pool 512 KiB |
| cFE SB | 547,980 | buffer pool 512 KiB, message map 16 KiB |
| cFE ES | 44,868 | |
| OSAL shared | 38,588 | printf buffer 17,300, object tables |
| OSAL BSP | 16,640 | BSP main task stack 16 KiB |
| everything else | 12,333 | main stack 4 KiB, EVS, FreeRTOS, TIME, newlib |

Measured after 20 s of counter_app (no ground commands sent):

| Item | Configured | Peak / in use |
|---|---|---|
| Task stacks in use (9 OSAL tasks + timebase + BSP main + console + idle + timer) | see report | largest: CFE_ES 2716 / 8192, CFE_SB 2620 / 8192, CFE_TIME 1764, CFE_TBL 1748, CFE_EVS 1732, BSP_MAIN 1296 / 16384, TIME_TONE 860 / 4096, COUNTER_APP 704 / 16384, cFS-Master timebase 364 / 8192, OS_CONSOLE 260 / 2048, Tmr_Svc 180 / 1024, IDLE 76 / 1024; MSP about 280 / 4096 |
| Stack pool slots | 4 KiB × 16, 8 KiB × 16, 16 KiB × 8, 32 KiB × 4 | used 2, 6, 1, 0 |
| SB buffer pool | 524,288 | high-water 1,264; SB peak in use 624 bytes, 3 buffers |
| TBL buffer pool | 524,288 | high-water 65,600 (TBL's 4 load buffers of 16 KiB, taken at start); 0 tables registered |
| SB message ids / pipes / subscriptions | 256 / 64 / 4096 | 15 / 5 / 15; deepest pipe peak 2 of 12 |
| CDS | 131,072 | not used by this mission |
| Reset area | 129,400 | = `sizeof(CFE_ES_ResetData_t)`, mostly the performance log (10,000 entries) |
| PSP volatile disk / user reserved | 2 MiB / 1 MiB | volatile disk mounted as `/ram`, no files written; user reserved unused |
| OSAL RAM-disk pool | 1 MiB | unused (cFE gives `/ram` its own memory) |
| OSAL queues | 64 × 8 KiB storage | 5 created, depth 32/32/12/12/12, 4-byte messages (needs (depth + 2) × 8 = 272 bytes) |
| OSAL objects | tasks 64, queues 64, mutexes 20, binsem 20, countsem 20, timebases 5, timers 10, streams 50, filesys 14 | 9, 5, **16**, 3, 0, 1, 1, 0, 2 |

Measured after the cFE functional tests (`--ft`; same cFE, OSAL and PSP code, test apps added, test sizes):

| Item | Peak / in use |
|---|---|
| cFE core task stacks | unchanged from the counter_app run (CFE_ES 2716, CFE_SB 2620, CFE_TIME 1764, CFE_TBL 1748, CFE_EVS 1732 of 8192) |
| ES_BG_TASK / TIME_ONEHZ_TASK / BSP_MAIN | 2032 / 4096, 1600 / 8192, 1600 / 16384 |
| test tasks (exited) | at most 3320 bytes |
| SB pool | high-water 35,364; SB peak in use 33,460 (the tests send large messages), 7 buffers |
| TBL pool | high-water 68,696 |
| SB pipes | peak 64 of 64 (a test fills the pipe table on purpose) |
| OSAL objects at the end | tasks 10, queues 6, mutexes 17 of 20, modules 4 |

## Conclusions (reasoned, to be confirmed in phase 2)

- About 7.3 MB is buffers that this mission does not use: PSP reserved memory (most of 4 MiB), the OSAL RAM-disk
  pool (1 MiB), the SB and TBL pools beyond their needs, unused stack-pool slots and queue storage.
- The image is built **without optimisation** (`-O0`: no `-O` flag in the cross toolchain or `CMAKE_BUILD_TYPE`).
  Stack figures are for `-O0`; another optimisation level changes them and needs a new measurement.
- The counter_app run sends no commands; the functional-test run exercises cFE's command and API paths and
  leaves the core tasks' stack peaks unchanged, so they come from start-up. Largest: 2716 bytes (CFE_ES).
- Mutexes (16 of 20) are close to their limit and should not be cut.

# Phase 2: flight sizes (2026-11-12)

Plan approved by the user 2026-11-12 (values in D36). Files: `config/cfe/freertos-arm-cm3/` `m2s_osal_sizing.cmake`,
`m2s_sizing.cmake`, `cfe_{es,sb,tbl,time}_internal_cfg.h`, `global_build_options.cmake`, startup script
(counter_app stack 4096); `freertos-arm-cm3-qemu-ft/` files of the same names keeping the default sizes;
`scripts/lib/cm3-image.sh` `check_ram_budget`, called by scripts 08 and 09.

Found on the way: cFE compiles code that includes the internal config headers in its host-side mission build as
well (`cfeconfig_platformdata_tool`), so the include path for the stock headers is set in
`global_build_options.cmake` (both build levels), not in `arch_build_custom.cmake` (first build failed:
`default_cfe_es_internal_cfg.h: No such file or directory`).

## Results (verified)

| Check | Result |
|---|---|
| `08 --clean`: build, image checks, no heap; RAM budget: .data + .bss + .main_stack = 488,116 bytes ≤ 524,288 (93 %); text 212,815, data 1,296, bss 486,820 | PASS (exit 0) |
| `RAM_BUDGET_BYTES=400000 ./scripts/08-build-cfe-arm-cm3.sh` | fails as intended (exit 1, "RAM budget exceeded: ... 488116 bytes > 400000") |
| Every override in effect (nm): PSP block 131,072; TBL global 83,120; SB global 68,264; stack pools 4 KiB × 12 + 8 KiB × 2; queue table 20,224; timebase table 11,980; printf buffer 11,072; RAM-disk pool 4,096; `/cf` root 16,384; BSP main stack 4,096 | PASS |
| `09 --clean`: RAM budget 488,108 bytes; counter_app 5 × 20 s under QEMU (max interval deviation 4.741 ms, drift within ±4.4 ms) | PASS 5/5 |
| `10-memory-report.sh --clean` at the new sizes: all stacks ≥ 25 % free (tightest CFE_ES 2712 / 4096 = 34 % free, CFE_SB 36 %, BSP_MAIN 68 %); SB pool high-water 1,264 of 65,536 (1.9 %); PSP reserved 104,832 of 131,072 used (reset area 21,700, volatile disk 65,536, CDS 16,384, user reserved 1,024); OSAL: tasks 9/24, queues 5/16, mutexes 16/20 | PASS |
| TBL pool high-water 32,800 of 65,536 (50.0 %, the two 16 KiB load buffers taken at start; 32,736 bytes left for tables, none in this mission) | just over the "≤ 50 %" criterion; accepted as a fixed start-up allocation, not traffic |
| `09c --clean` (test sizes): 3/3 rounds, 2521 assertions equal to native (2044 PASS, 0 FAIL, 13 MIR) | PASS |
| `09b` (OSAL tests, unchanged config): 75/75 runs | PASS |
| `06` simulator counter_app 5/5; `05b` simulator functional tests 3/3 (same as native except the 1 known D29 difference) | PASS |
| `07-check-bundle` | **FAIL (exit 1)**, see below |

Static RAM by owner after the cut (488,063 bytes): PSP 131,388; OSAL FreeRTOS layer 128,086; cFE SB 87,724;
cFE TBL 83,120; OSAL shared 24,992; cFE ES 16,068; OSAL BSP 4,352; main stack 4,096; rest 8,237.

## Option A (user decision 2026-11-12): bundle sources unchanged

`07-check-bundle` failed (below). The user chose option A: leave the bundle's sources unchanged. The
`BSP_MAIN_TASK_STACK_BYTES` override in `osal-ext/freertos-arm-cm3/src/bsp_start.c` was removed again (file back
to its committed content) and the BSP main task keeps its 16 KiB stack (+12 KiB). The local tag `v2-sim-qemu` in
the generated clone `build/export/cfs-m2s-freertos` was reset to the published `ed41597` (approved with option A).

| Check (after option A) | Result |
|---|---|
| `08 --clean`: RAM budget .data + .bss + .main_stack = 500,404 bytes ≤ 524,288 (95 %) | PASS (exit 0) |
| `09 --clean`: RAM budget 500,396 bytes; counter_app | **4/5 (exit 1)**: run 3, one counter line 90 ms late and the next as early |
| `09` again | **4/5 (exit 1)**: run 5, line 16 at +132.8 ms, line 17 at −135.6 ms; drift 1.0 ms |
| `RUNS=10 ./scripts/09-run-cfe-arm-cm3-qemu.sh` | PASS 10/10 (max deviation 7.4 ms) |
| `10-memory-report.sh --clean`: BSP_MAIN 1296 / 16384; other stacks as before (CFE_ES 2712 / 4096, 34 % free); static RAM 500,351 bytes | PASS |
| `07-check-bundle` | FAIL (exit 1), same cause as below — not caused by step 10 |

The two 09 failures (2 of 20 runs) have the host-side signature: one counter line late and the next one early by
the same amount, total drift ≤ 1.5 ms, so the guest kept its 1 s period and the output reached the host late.
The only change between the 5/5 run and these was the BSP main stack size (memory layout only). Reasoned: host
timing on this 2-vCPU container (load 1.4–3.4), as K4 on the VirtualBox VM; not proven.

## 07-check-bundle failure (open, not caused by step 10)

The bundle includes `osal-ext/freertos-arm-cm3`, so the `bsp_start.c` change reaches it: the first export made a
new local bundle commit (`256a72d` on the published `ed41597`: `bsp_start.c` +3 lines, `PORTING.md` source
revision) and moved the local tag `v2-sim-qemu` onto it (`git tag -f`). The check's second export then fetches
the published tags, and git refuses to overwrite the local `v2-sim-qemu` ("would clobber existing tag"), so the
export stops with "cannot fetch". Not a sizing defect: the export script cannot yet produce a bundle whose
content differs from the published version under the same tag.

After option A the export still makes a new commit (`ce249fd`, 1 file): `PORTING.md` records the source revision
of m2s-cfs (`Exported from source revision ...`), published as `6df949034fb9-dirty`, now `db923ae514a2`. So every
m2s-cfs commit after the published export changes the bundle, and `07-check-bundle` has failed this way since
those commits (7b5a7de, db923ae), independent of step 10. Fixing it means changing the export/check scripts (step
07 territory) and is left to the user.
