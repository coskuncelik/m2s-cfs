# Running the Cortex-M3 images in QEMU by hand

How to watch the emulated Cortex-M3 (QEMU machine `mps2-an385`) in your own terminal, outside the test scripts.
The scripts (09, 09b, 09c) run QEMU unattended and check the output; this page is for looking at it yourself.

Prerequisites: `qemu-system-arm` (QEMU 8.2.2, see `docs/VERSIONS.md`; script 01 prints the install command) and
an image built by the matching script (the commands below use the paths those scripts build).

## The command

Run QEMU in the **foreground** of your terminal:

```
qemu-system-arm -M mps2-an385 -icount shift=auto,sleep=on,align=on -nographic -kernel <image>
```

| Option | Meaning |
|---|---|
| `-M mps2-an385` | ARM MPS2 board with a Cortex-M3 (our board `boards/qemu-mps2-an385`) |
| `-icount shift=auto,sleep=on,align=on` | guest time follows real time (D32); without it SysTick ticks get lost on a busy host and the 1 s counter runs slow |
| `-nographic` | no window; the board's UART0 (our console) goes to your terminal, together with QEMU's console multiplexer |
| `-kernel <image>` | the ELF file to load; the CPU starts from its vector table |
| `-semihosting-config enable=on,target=native` | only for test images: lets the program end QEMU with its exit status (D33) |

**To quit: press Ctrl-A, then X.** (Ctrl-C goes to the emulated board, not to QEMU. Ctrl-A H lists the other
keys.) QEMU prints `QEMU: Terminated`.

## counter_app (step 09)

Build once with `./scripts/09-run-cfe-arm-cm3-qemu.sh --clean` (or without `--clean` afterwards), then:

```
cd build/cfe-freertos-arm-cm3-qemu/exe/cpu1
qemu-system-arm -M mps2-an385 -icount shift=auto,sleep=on,align=on -nographic -kernel core-cpu1
```

You see the PSP start-up, cFE's start-up messages, `CFE_ES_Main entering OPERATIONAL state`,
`counter_app initialized`, then `counter_app counter: 1`, `2`, ... once per second. It runs until you press
Ctrl-A X.

## An OSAL test program (step 09b)

Build with `./scripts/09b-run-osal-tests-arm-cm3-qemu.sh`, then for example:

```
cd build/osal-freertos-arm-cm3/tests
qemu-system-arm -M mps2-an385 -icount shift=auto,sleep=on,align=on -semihosting-config enable=on,target=native -nographic -kernel queue-test
echo $?
```

The test prints its assertions and summary, then `M2S TEST HEAP used ...` and `M2S EXIT status 0`, and QEMU ends
by itself with that status (`echo $?` prints it). The 25 test programs are in `build/osal-freertos-arm-cm3/tests/`
and `build/osal-freertos-arm-cm3/unit-tests/*/` (list in `scripts/lib/osal-tests.sh`).

## cFE functional tests (step 09c)

Build with `./scripts/09c-run-cfe-functional-tests-arm-cm3-qemu.sh`, then:

```
cd build/cfe-freertos-arm-cm3-qemu-ft/exe/cpu1
qemu-system-arm -M mps2-an385 -semihosting-config enable=on,target=native -nographic -kernel core-cpu1
```

No `-icount` here, as in script 09c (D34): with it a run takes about 25 minutes instead of about 3. The tests run
for about 2–3 minutes, then `ft_logdump` prints the test log as `FTLOG|...` lines between `FT_LOGDUMP: BEGIN` and
`FT_LOGDUMP: END <bytes>`. The program does not exit: press Ctrl-A X after the END line.

## Lines to look out for

| Line | Meaning |
|---|---|
| `M2S TRAP ...` | dynamic allocation attempted, test heap exhausted, or a FreeRTOS stack overflow; the CPU halted |
| `M2S FAULT exception N CFSR ... PC ...` | CPU fault; PC is the faulting address (`arm-none-eabi-addr2line -e <image> <PC>`) |
| `M2S ASSERT <file>:<line>` | FreeRTOS `configASSERT` failed |
| `M2S PANIC code ...` | `CFE_PSP_Panic` |
| `M2S STOP ...` / `M2S EXIT status N` | the program stopped / ended with status N |

After a halt, QEMU keeps running with the CPU stopped: quit with Ctrl-A X.

## Why the scripts do it differently

The scripts start QEMU under `timeout`, which puts it in a background process group. A background process that
reads the terminal is stopped by the system (SIGTTIN), and `-serial stdio` reads standard input, so the scripts give
QEMU `/dev/null` as input and save the output in `logs/` (fix of 2026-11-07, see
`docs/test-results/09-cfe-arm-cm3-qemu.md`). In the foreground, as above, QEMU may read the terminal, so nothing
special is needed. To follow a script's run live instead, use `tail -f logs/09-qemu-runs/run1.log` in a second
terminal.

## Debugging with gdb

Add `-s -S` to the QEMU command (gdb server on TCP port 1234, CPU halted at reset), then in another terminal:

```
gdb-multiarch <image> -ex 'target remote :1234'
```

For example `break main`, `continue`, `bt`, or `watch *(unsigned int *)0x3c` (the SysTick vector) as used to find
the `.got` problem in step 09c.
