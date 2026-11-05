# cfs-m2s-freertos

NASA's core Flight System (cFE, OSAL, PSP) running on **FreeRTOS V11.3.2**. This bundle targets the Linux
simulator: the real FreeRTOS kernel with its POSIX port, running in a Linux process. The native POSIX OSAL is
not used. The layout is the stock cFS bundle (`nasa/cFS`), with the FreeRTOS port added where cFE and OSAL
look for platforms. NASA's original README is `README-cFS.md`.

The FreeRTOS OS layer, the PSP time base and `counter_app` are written for the next target as well: a
SmartFusion2 board (ARM Cortex-M3, FreeRTOS `GCC/ARM_CM3`). That board port is not in this bundle yet.

## Prerequisites

Linux x86-64 host with `gcc`, `make`, `cmake` (3.19 or later), `patch`, `xxd` and GNU `tar` (1.28 or later).

## Build and run

```
make prep          # configure (mission config: sample_defs/)
make               # build
make install       # install to build/exe/cpu1
cd build/exe/cpu1
./core-cpu1        # stop with Ctrl-C
```

`core-cpu1` starts cFE, which starts `counter_app` from the embedded startup script. The app prints:

```
counter_app initialized
counter_app counter: 1
counter_app counter: 2
...
```

There is one line per second, on fixed 1 s deadlines, so the counter does not drift. All cFE output goes
through the OSAL console to the terminal. After the first `make prep`, a plain `make` re-runs the configure
step when needed, for example after editing `sample_defs/cpu1_cfe_es_startup.scr`.

## What is where

| Part | Location |
|---|---|
| OSAL OS layer for FreeRTOS (static memory only) | `osal/src/os/freertos/` |
| OSAL BSP for the Linux simulator (`main()`, console, `FreeRTOSConfig.h`, kernel build, POSIX port patch) | `osal/src/bsp/freertos-posix-sim/` |
| cFE PSP for the simulator | `psp/fsw/freertos-posix-sim/` |
| PSP time base from the FreeRTOS tick | `psp/fsw/modules/timebase_freertos/` |
| Example app | `apps/counter_app/` |
| Mission configuration (one CPU, `counter_app` linked statically) | `sample_defs/` |
| FreeRTOS kernel, unmodified | `FreeRTOS-Kernel/` |

`PORTING.md` describes the design and lists every added or changed file.

## Static memory

All FreeRTOS objects are created with the `*Static` APIs. `configSUPPORT_DYNAMIC_ALLOCATION` is 0, and no
FreeRTOS heap is linked. The OS layer, BSP, PSP, cFE and the app reference no `malloc`/`free`; on the
simulator, only the FreeRTOS POSIX port does (`port.c`, `wait_for_event.c`), for host threads. Check with:

```
for o in $(find build/freertos-posix-sim -name '*.o' -path '*CMakeFiles*' ! -path '*ut-stubs*' ! -path '*ut_assert*'); do
    nm -u "$o" | grep -qE '\b(malloc|free)$' && echo "$o"
done
```

It lists only `port.c.o` and `wait_for_event.c.o`. OSAL's test library `ut_assert`, which uses `malloc`,
is not linked into `core-cpu1`.

## Limitations (simulator)

- Tasks run on host thread stacks, not on the stack memory OSAL hands to FreeRTOS. Stack sizes and stack
  addresses are therefore not representative of a real target.
- The process is pinned to one host CPU (FreeRTOS here is single-core).
- Timing depends on the host: a heavily loaded host can delay the 1 ms tick.
- Not supported by the FreeRTOS OS layer: network and sockets, loading modules from files (apps are linked
  statically), condition variables, the shell.
- Every start is a power-on reset; processor resets are not simulated.

## License

See `NOTICE`. cFS is Apache-2.0, FreeRTOS-Kernel is MIT, and the added FreeRTOS port is Apache-2.0.
