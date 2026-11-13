# cfs-m2s-freertos

NASA's core Flight System (cFE, OSAL, PSP) running on **FreeRTOS V11.3.2**, in the layout of the stock cFS
bundle (`nasa/cFS`), with the FreeRTOS port added where cFE and OSAL look for platforms. NASA's original README
is `README-cFS.md`.

Two mission configurations are included. cFE selects one with `MISSIONCONFIG=<name>` (folder `<name>_defs`):

| Configuration | Target | Status |
|---|---|---|
| `freertos_sim` | Linux simulator: the real FreeRTOS kernel with its POSIX port, in a Linux process | builds and runs; commands and telemetry over UDP with the included ground station |
| `freertos_qemu` | ARM Cortex-M3 (FreeRTOS `GCC/ARM_CM3` port), emulated by QEMU (machine `mps2-an385`) | builds and runs under QEMU |

Both run the same cFE, OSAL OS layer, `sample_lib` and `counter_app`; only the BSP, PSP and FreeRTOS port
differ. The simulator also runs NASA's `sch_lab`, `ci_lab` and `to_lab`, so it can be commanded and monitored
over UDP (`ground_station/`). The target hardware is a SmartFusion2 M2S150 board (Cortex-M3); its configuration
is not in this bundle yet.

## Prerequisites

- Both: Linux x86-64 host with `make`, `cmake` (3.19 or later), `xxd` and GNU `tar` (1.28 or later).
- `freertos_sim`: `gcc` and `patch`; Python 3 for the ground station (standard library only).
- `freertos_qemu`: the cross compiler `arm-none-eabi-gcc` with newlib-nano, and `qemu-system-arm` to run it.
  On Ubuntu 24.04: `sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi qemu-system-arm`
  (tested with arm-none-eabi-gcc 13.2.1, newlib 4.4.0, QEMU 8.2.2).

## Build and run

Give every configuration its own build folder with `O=`. With more than one `*_defs` folder, cFE needs
`MISSIONCONFIG` on the `prep` step; later `make` calls with the same `O=` remember it.

### Linux simulator (`freertos_sim`)

```
make MISSIONCONFIG=freertos_sim O=build-sim prep
make O=build-sim
make O=build-sim install
cd build-sim/exe/cpu1
./core-cpu1                 # stop with Ctrl-C
```

### Cortex-M3 on QEMU (`freertos_qemu`)

```
make MISSIONCONFIG=freertos_qemu O=build-qemu prep
make O=build-qemu
make O=build-qemu install
cd build-qemu/exe/cpu1
qemu-system-arm -M mps2-an385 -icount shift=auto,sleep=on,align=on -nographic -kernel core-cpu1
```

**To quit QEMU: press Ctrl-A, then X.** (Ctrl-C goes to the emulated board.) `-icount ... align=on` keeps the
emulated time in step with real time; without it QEMU can lose timer ticks on a busy host and the counter runs
slow. `-nographic` sends the board's UART to your terminal. Run QEMU in the foreground like this: started in
the background (e.g. under `timeout`) it would be stopped when it reads the terminal, so give it
`</dev/null` there.

### What you should see

In both configurations `core-cpu1` starts cFE, which loads `sample_lib` and starts `counter_app` (and on the
simulator `ci_lab`, `to_lab`, `sch_lab`) from the embedded startup script. Among cFE's start-up messages:

```
EVS Port1 ... 66/1/COUNTER_APP 1: counter_app initialized
EVS Port1 ... 66/1/COUNTER_APP 8: Table loaded: period 1000 ms, limits 500..5000 ms
EVS Port1 ... 66/1/COUNTER_APP 2: File test passed on /ram/counter: wrote 256, read back, appended to 512 bytes, listed, renamed, removed
counter_app file test: PASS
SAMPLE_LIB_Function called, buffer='SAMPLE DATA'
counter_app sample_lib call: PASS
EVS Port1 ... 66/1/COUNTER_APP 18: Worker task and timer started: period 1000 ms, timer accuracy 1000 us
CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state
counter_app counter: 1
counter_app counter: 2
...
```

There is one counter line per second: an OSAL timer wakes counter_app's worker task, and the timer's time base
runs on absolute tick deadlines, so the counter does not drift. `EVS Port1` lines are cFE events printed on the
console. All cFE output goes through the OSAL console to the terminal (simulator) or the UART (Cortex-M3). After the first `prep`, a plain
`make O=...` re-runs the configure step when needed, for example after editing the startup script
`<name>_defs/cpu1_cfe_es_startup.scr`.

Coming from a previous version of this bundle: delete the old build folders (`build/`, `build-sim/`,
`build-qemu/`) and run `prep` again, because the configurations now have more apps and the simulator has
networking on.

### Commands and telemetry (simulator)

While `core-cpu1` of `freertos_sim` runs, in a second terminal:

```
python3 ground_station/ground_station.py interactive
> tlm-enable          # to_lab starts sending telemetry to 127.0.0.1:1235
> noop                # counter_app NOOP command (via ci_lab, UDP 1234)
> set-period 2000     # counter_app counts every 2 s
> quit
```

`ground_station/README.md` lists all commands and the telemetry it decodes.

## counter_app

An example cFE app, the same source on both targets:

- **Events** (EVS) for start-up, its tests and its commands; IDs in `fsw/src/counter_app_events.h`.
- **Start-up file test** on cFE's RAM disk `/ram` with the OSAL file API (create, write, read back, append,
  list, rename, remove), and a read of the startup script from `/cf`.
- **Library call** to NASA's `sample_lib`, loaded by cFE before the app.
- **Commands** (message ID `0x1890`): NOOP, RESET_COUNTERS, SET_PERIOD (within the table's limits), with length
  and code checks; **housekeeping telemetry** (`0x0890`) on each request (`sch_lab` sends one per second on the
  simulator). Formats: `fsw/inc/counter_app_msgids.h`, `counter_app_msg.h`.
- **Parameter table** `Params` (default period, SET_PERIOD limits), loaded from memory at start-up, with a
  validation function (`fsw/inc/counter_app_tbl.h`).
- **Worker task and timer:** an OSAL timer gives a binary semaphore each period; a child task takes it and
  counts under a mutex shared with the main task, which serves the commands.

## What is where

| Part | Location |
|---|---|
| OSAL OS layer for FreeRTOS (static memory only), both targets | `osal/src/os/freertos/` |
| OSAL BSP for the Linux simulator (`main()`, console, `FreeRTOSConfig.h`, kernel build, POSIX port patch) | `osal/src/bsp/freertos-posix-sim/` |
| OSAL BSP for the Cortex-M3 (`main()`, console, `FreeRTOSConfig.h`, kernel build, compiler settings) | `osal/src/bsp/freertos-arm-cm3/` |
| Cortex-M3 start-up, newlib stubs, allocator traps, fault report; the QEMU board | `osal/src/bsp/freertos-arm-cm3/boards/` |
| cFE PSPs (simulator, Cortex-M3) | `psp/fsw/freertos-posix-sim/`, `psp/fsw/freertos-arm-cm3/` |
| PSP time base from the FreeRTOS tick | `psp/fsw/modules/timebase_freertos/` |
| Example app | `apps/counter_app/` |
| Mission configurations (one CPU, apps linked statically) | `freertos_sim_defs/`, `freertos_qemu_defs/` |
| `sch_lab` and `to_lab` tables of the simulator configuration | `freertos_sim_defs/tables/` |
| Ground station (Python, host side) | `ground_station/` |
| FreeRTOS kernel, unmodified | `FreeRTOS-Kernel/` |

`PORTING.md` describes the design and lists every added or changed file.

## Static memory

All FreeRTOS objects are created with the `*Static` APIs. `configSUPPORT_DYNAMIC_ALLOCATION` is 0, and no
FreeRTOS heap is linked. The OS layer, BSPs, PSPs, cFE and the app reference no `malloc`/`free`.

- **Cortex-M3:** no allocator is linked at all. `malloc`, `free`, `calloc`, `realloc` and newlib's `_r` forms
  are traps (`boards/common-cm3/alloc_traps.c`): a call prints `M2S TRAP ...` on the console and stops the CPU.
  The link map (`core-cpu1.map` in the build folder) shows that newlib's allocator objects are absent. `stdout`
  is unbuffered, so newlib's stdio never allocates a buffer.
- **Simulator:** only the FreeRTOS POSIX port uses `malloc` (`port.c`, `wait_for_event.c`), for host threads.
  Host sockets (simulator networking) use host kernel memory, also on the host side. Check with:

```
for o in $(find build-sim/freertos-posix-sim -name '*.o' -path '*CMakeFiles*' ! -path '*ut-stubs*' ! -path '*ut_assert*'); do
    nm -u "$o" | grep -qE '\b(malloc|free)$' && echo "$o"
done
```

It lists only `port.c.o` and `wait_for_event.c.o`. OSAL's test library `ut_assert`, which uses `malloc`,
is not linked into `core-cpu1`.

## Messages from the Cortex-M3 image

| Line | Meaning |
|---|---|
| `M2S TRAP ...` | dynamic allocation attempted, or a FreeRTOS stack overflow; the CPU stopped |
| `M2S FAULT exception N CFSR ... PC ...` | CPU fault; `arm-none-eabi-addr2line -e core-cpu1 <PC>` gives the source line |
| `M2S ASSERT <file>:<line>` | FreeRTOS `configASSERT` failed |
| `M2S PANIC code ...` | `CFE_PSP_Panic` |

Debugging: add `-s -S` to the QEMU command (gdb server on port 1234, CPU halted at reset), then
`gdb-multiarch core-cpu1 -ex 'target remote :1234'`.

## Limitations

Simulator:

- Tasks run on host thread stacks, not on the stack memory OSAL hands to FreeRTOS. Stack sizes and stack
  addresses are therefore not representative of a real target.
- The process is pinned to one host CPU (FreeRTOS here is single-core).
- Timing depends on the host: a heavily loaded host can delay the 1 ms tick.
- Networking uses host sockets. A FreeRTOS task blocked in a host system call would stop every lower-priority
  task (the FreeRTOS scheduler does not see the block), so every socket wait polls: a zero-timeout `select()`,
  then a one-tick FreeRTOS delay. Latency is at most one tick (1 ms).
- `ci_lab` accepts commands from any UDP sender (NASA lab app, no authentication): local use only.

Cortex-M3 on QEMU:

- QEMU is not real hardware: no caches, no flash or DDR timing, simplified peripherals. The console is a polled
  UART (CMSDK UART0 at `0x40004000`).
- The memory sizes are those of the simulator (about 7.6 MB of static data, in the emulated 16 MiB PSRAM);
  they are not yet sized for a board.

Both:

- Not supported by the FreeRTOS OS layer: loading modules from files (apps are linked statically), condition
  variables, the shell; network and sockets on the Cortex-M3 (the simulator has them through host sockets).
- Every start is a power-on reset; processor resets are not preserved.

## License

See `NOTICE`. cFS is Apache-2.0, FreeRTOS-Kernel is MIT, and the added FreeRTOS port is Apache-2.0.
