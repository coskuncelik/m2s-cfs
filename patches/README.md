# Patches

Project changes to pinned upstream sources. Files in `sources/` are never edited: each patch is
applied at build time to a copy in the build directory (`patch -o`).

| Patch | Applies to | Used by | Why |
|---|---|---|---|
| `freertos-kernel/0001-posix-port-tick-catch-up.patch` | FreeRTOS-Kernel `8a59a4b` (V11.3.2), `portable/ThirdParty/GCC/Posix/port.c` | Linux simulator only | The stock tick thread (`usleep(1 ms)` loop) runs FreeRTOS time at 0.38–0.44× real time on the development VM, and coalesced SIGALRMs lose ticks. The patch sleeps to absolute deadlines and processes every due tick from `CLOCK_MONOTONIC`. See `docs/DECISIONS.md` D14. |

If a pinned revision changes (only with user approval), every patch for it must be re-checked.
