# Ground station (m2s-cfs simulator)

A minimal ground station for cFS on the FreeRTOS Linux simulator: Python 3, standard library only, no install.
Commands go over UDP to `ci_lab` (default `127.0.0.1:1234`); telemetry comes from `to_lab` (UDP port `1235`)
after the `tlm-enable` command.

## Use

Build and start the simulator (from the repository root):

    ./scripts/06-run-counter-app-sim.sh          # builds build/cfe-freertos-sim (and runs its checks)
    cd build/cfe-freertos-sim/exe/cpu1 && ./core-cpu1

In a second terminal:

    python3 tools/ground_station/ground_station.py interactive --log gs.log
    > tlm-enable                 # to_lab starts sending telemetry to 127.0.0.1
    > noop                       # counter_app NOOP (event 10, command counter + 1)
    > set-period 2000            # counter_app period 2 s (limits 500..5000 ms from its table)
    > reset                      # counter_app command and error counters to 0
    > es-noop                    # cFE Executive Services NOOP
    > raw 0x1890 9               # any command: MID, function code, optional payload in hex
    > quit

or one command at a time (`send`) and a separate telemetry window (`listen`):

    python3 tools/ground_station/ground_station.py listen --log gs.log
    python3 tools/ground_station/ground_station.py send set-period 1500

Global options go before the mode: `--host` (command destination), `--cmd-port`, `--tlm-port`.

Each telemetry line: host time, message ID, length, and for counter_app housekeeping (`COUNTER_HK counter=…
period=… lib=… cmdcnt=… errcnt=… filetest=…`) and event messages (`EVENT <app> <id> <type>: <text>`) the
decoded fields.

## Notes

- Packet formats are written out in `ground_station.py` (docstring) and must follow counter_app's
  `fsw/inc/counter_app_msgids.h` and `counter_app_msg.h`; `scripts/12c-run-ground-station-sim.sh` checks the
  command bytes against cFE's `cmdUtil` and every command's effect in the telemetry.
- `ci_lab` accepts commands from any UDP sender (NASA lab app, no authentication): for local simulator use only.
