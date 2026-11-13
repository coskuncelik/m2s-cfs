# Ground station

A minimal ground station for cFS on the FreeRTOS Linux simulator (configuration `freertos_sim`): Python 3,
standard library only, nothing to install. Commands go over UDP to `ci_lab` (default `127.0.0.1:1234`);
telemetry comes from `to_lab` (UDP port `1235`) after the `tlm-enable` command.

## Use

Build and start the simulator (see the top-level `README.md`), e.g.:

    make MISSIONCONFIG=freertos_sim O=build-sim prep && make O=build-sim && make O=build-sim install
    cd build-sim/exe/cpu1 && ./core-cpu1

In a second terminal, from the top of the repository:

    python3 ground_station/ground_station.py interactive --log gs.log
    > tlm-enable                 # to_lab starts sending telemetry to 127.0.0.1
    > noop                       # counter_app NOOP (event 10, command counter + 1)
    > set-period 2000            # counter_app period 2 s (limits 500..5000 ms from its parameter table)
    > reset                      # counter_app command and error counters to 0
    > es-noop                    # cFE Executive Services NOOP
    > raw 0x1890 9               # any command: message ID, function code, optional payload in hex
    > quit

or one command at a time (`send`) and a separate telemetry window (`listen`):

    python3 ground_station/ground_station.py listen --log gs.log
    python3 ground_station/ground_station.py send set-period 1500

Global options go before the mode: `--host` (command destination), `--cmd-port`, `--tlm-port`.

Each telemetry line shows the host time, message ID and length, and for counter_app housekeeping
(`COUNTER_HK counter=… period=… lib=… cmdcnt=… errcnt=… filetest=…`) and event messages
(`EVENT <app> <id> <type>: <text>`) the decoded fields.

## Notes

- The packet formats are written out in `ground_station.py` and follow counter_app's
  `apps/counter_app/fsw/inc/counter_app_msgids.h` and `counter_app_msg.h`; keep them in step when changing the app.
- `ci_lab` accepts commands from any UDP sender (NASA lab app, no authentication): for local simulator use only.
