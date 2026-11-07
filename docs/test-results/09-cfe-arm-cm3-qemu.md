# Test result — 09 cFE + counter_app on an emulated Cortex-M3 (QEMU mps2-an385)

- Date: 2026-11-06
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs), cmake 3.28.3
- Tools (see `docs/VERSIONS.md`): `arm-none-eabi-gcc` 13.2.1, binutils 2.42, newlib-nano 4.4.0, QEMU 8.2.2
  (`qemu-system-arm`), gdb-multiarch 15.1
- Sources (unmodified in `sources/`): cFS `c62b5b9`, FreeRTOS-Kernel `8a59a4b` (V11.3.2, `GCC/ARM_CM3` port)
- Board: `boards/qemu-mps2-an385` (emulated ARM MPS2 AN385, not the SmartFusion2) + `boards/common-cm3`
- Decision: D32.
- Command: `./scripts/09-run-cfe-arm-cm3-qemu.sh --clean` (RUNS=5, RUN_SECONDS=20, TOLERANCE_MS=50, DRIFT_MS=10;
  QEMU `-icount shift=auto,sleep=on,align=on`). Exit 0.

## Results

| Check | Result |
|---|---|
| QEMU `mps2-an385` memory map (`info mtree`): SSRAM1 4 MiB at 0, PSRAM 16 MiB at `0x21000000`, UART0 at `0x40004000`; clock 25 MHz (`info qtree`) | PASS |
| Build and install with cFE's strict flags, no compiler warnings | PASS |
| Image: ELF32 ARM EABI5, ARMv7-M Thumb-2; entry = `Reset_Handler` (`0x000246ec`); vectors 0, 1, 11, 14, 15 correct; link map (7317 lines) | PASS |
| No newlib allocator object linked, allocator entry points only as the `syscalls.c` traps, `_sbrk` discarded; no FreeRTOS heap; no target object references `malloc`/`free` (0 of 151) | PASS |
| 5 runs × 20 s: ES loads `COUNTER_APP`, OPERATIONAL, "counter_app initialized" once, counter 1..19 without gap, every interval 1 s ± 50 ms, drift ≤ 10 ms, no error line, no `M2S TRAP/FAULT/ASSERT/PANIC/STOP` line | PASS — 5/5 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |
| Trap path (manual, gdb-multiarch on QEMU's gdb server): `call malloc(64)` while counter_app runs → UART shows `M2S TRAP malloc size 64 (dynamic allocation is not allowed)`, the counter stops (CPU halted); script 09's error pattern matches that line | PASS |
| Regression (shared start-up, stubs, CMake and script libraries changed): `08 --clean` exit 0, `06 --clean` 5/5, `05` 5/5, `07-check-bundle` exit 0 (counter_app 5/5) | PASS |

| Run | Counter lines | Max interval deviation | Drift over 18 s |
|---|---|---|---|
| 1 | 19 | 5.869 ms | −1.726 ms |
| 2 | 19 | 4.242 ms | −3.086 ms |
| 3 | 19 | 3.222 ms | +2.725 ms |
| 4 | 19 | 2.425 ms | +0.576 ms |
| 5 | 19 | 2.390 ms | −0.039 ms |

Image (`arm-none-eabi-size`): text 213409, data 1296, bss 7568796 bytes.

First executed Cortex-M3 code of the project: the generic start-up (including the boot-loader quiesce), the
FreeRTOS `ARM_CM3` port (SVC start, PendSV context switches, SysTick), OSAL tasks on their own static stacks, the
PSP `/cf` image, cFE start-up to OPERATIONAL and `counter_app`, all printing through OSAL console → BSP → UART0.
No allocator trap or `_sbrk` trap was hit at run time (they print an `M2S TRAP` line), so `stdout` stayed
unbuffered and nothing asked for heap memory.

## Problems found and fixed on the way

- QEMU timing (default mode): all 5 runs failed the timing checks (16–18 counter lines in 20 s, drift +1.0 to
  +1.9 s, intervals up to 308 ms late); functionally every run was correct. One 20 s run per mode, same image:

  | QEMU options | Host s per guest s | Max deviation | Drift |
  |---|---|---|---|
  | default | 1.057 | 84 ms | +1090 ms |
  | `-icount shift=auto,sleep=on` | 0.995 | 169 ms | −94 ms |
  | `-icount shift=auto,sleep=on,align=on` | 1.000 | 3.0 ms | +0.1 ms |

  Reasoned cause (not proven): in the default mode SysTick runs on the host clock; a tick that fires late on a
  busy host (load average 3.6 on 2 vCPUs) restarts the next period from the late moment, so the lost time is never
  made up and FreeRTOS time falls behind. With icount the guest clock comes from the instruction count and
  `align=on` holds it to real time. Approved: icount option, timing limits unchanged (D32).
- Script library: `cfe_sim_init` sets the host `nm`/`size`, so the shared Cortex-M3 checks
  (`scripts/lib/cm3-image.sh`) are sourced after it.

## Not covered

- QEMU is not the SmartFusion2: no caches, no flash or DDR timing, simplified peripherals; QEMU timing shows the
  software keeps time against its tick, not real-hardware timing. Memory sizes are not yet cut for a board.
- The vector-table copy to eSRAM and VTOR above `0x40000000` (SmartFusion2 DDR) are not exercised: on QEMU the
  table is at 0.
- OSAL functional tests and cFE functional tests on the Cortex-M3 (including D22's osal-core-test 02.003, which
  should pass where a task runs on its own stack) are not run yet (possible step 09b).

## Fix 2026-11-07: QEMU stopped when the script ran in a terminal

Reported by the user: `./scripts/09-run-cfe-arm-cm3-qemu.sh` in an interactive terminal failed 0/5 (empty run logs,
"missing line ... OPERATIONAL"; plus `counter-check.sh: line 72: values: bad array subscript`). The runs here had
passed because the agent's shell has no terminal on standard input.

- Cause: QEMU's `-serial stdio` reads standard input; `timeout` runs QEMU in its own background process group, and
  a background read from the terminal stops the process (SIGTTIN). Reproduced with a pseudo-terminal (`script`):
  0 bytes of output with the terminal as stdin, output at once with stdin from `/dev/null`.
- Fix: QEMU (and every command run by `scripts/lib/counter-check.sh`) gets stdin from `/dev/null`; also in
  script 09b and, explicitly, `scripts/lib/cfe-ft.sh`. The summary line no longer indexes an empty counter list.
- Verified with a terminal attached (`script -qec`): 09 5/5, 09b 75/75, 09c 3/3, 06 5/5, 05b 3/3, all exit 0.
