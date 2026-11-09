# cfs-m2s-freertos

NASA's core Flight System (cFE, OSAL, PSP) running on **FreeRTOS V11.3.2**, in the layout of the stock cFS
bundle (`nasa/cFS`), with the FreeRTOS port added where cFE and OSAL look for platforms. NASA's original README
is `README-cFS.md`.

Two mission configurations are included. cFE selects one with `MISSIONCONFIG=<name>` (folder `<name>_defs`):

| Configuration | Target | Status |
|---|---|---|
| `freertos_sim` | Linux simulator: the real FreeRTOS kernel with its POSIX port, in a Linux process | builds and runs |
| `freertos_qemu` | ARM Cortex-M3 (FreeRTOS `GCC/ARM_CM3` port), emulated by QEMU (machine `mps2-an385`) | builds and runs under QEMU |

Both run the same cFE, OSAL OS layer and `counter_app`; only the BSP, PSP and FreeRTOS port differ. The
target hardware is a SmartFusion2 M2S150 board (Cortex-M3); its configuration is not in this bundle yet.

## Prerequisites

- Both: Linux x86-64 host with `make`, `cmake` (3.19 or later), `xxd` and GNU `tar` (1.28 or later).
- `freertos_sim`: `gcc` and `patch`.
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

In both configurations `core-cpu1` starts cFE, which starts `counter_app` from the embedded startup script:

```
CFE_ES_Main: CFE_ES_Main entering OPERATIONAL state
counter_app initialized
counter_app counter: 1
counter_app counter: 2
...
```

There is one line per second, on fixed 1 s deadlines, so the counter does not drift. All cFE output goes
through the OSAL console to the terminal (simulator) or the UART (Cortex-M3). After the first `prep`, a plain
`make O=...` re-runs the configure step when needed, for example after editing the startup script
`<name>_defs/cpu1_cfe_es_startup.scr`.

Coming from the previous version of this bundle (one `sample_defs` folder): delete the old `build/` folder,
because cFE's build folder remembers the configuration folder it was made with.

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
| Mission configurations (one CPU, `counter_app` linked statically) | `freertos_sim_defs/`, `freertos_qemu_defs/` |
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
  Check with:

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

Cortex-M3 on QEMU:

- QEMU is not real hardware: no caches, no flash or DDR timing, simplified peripherals. The console is a polled
  UART (CMSDK UART0 at `0x40004000`).
- The memory sizes are those of the simulator (about 7.6 MB of static data, in the emulated 16 MiB PSRAM);
  they are not yet sized for a board.

Both:

- Not supported by the FreeRTOS OS layer: network and sockets, loading modules from files (apps are linked
  statically), condition variables, the shell.
- Every start is a power-on reset; processor resets are not preserved.

## License

See `NOTICE`. cFS is Apache-2.0, FreeRTOS-Kernel is MIT, and the added FreeRTOS port is Apache-2.0.
