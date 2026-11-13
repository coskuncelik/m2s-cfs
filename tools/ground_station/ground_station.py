#!/usr/bin/env python3
"""ground_station.py — minimal ground station for the m2s-cfs Linux simulator (step 12c, D43).

Talks to cFS over UDP: commands go to ci_lab (default 127.0.0.1:1234), telemetry comes from to_lab (default
UDP port 1235, after the "tlm-enable" command). Python 3 standard library only.

Usage:
  ground_station.py listen [--seconds N] [--log FILE]      print (and log) decoded telemetry
  ground_station.py send tlm-enable [IP]                   to_lab OUTPUT_ENABLE (default IP 127.0.0.1)
  ground_station.py send noop | reset | es-noop
  ground_station.py send set-period MS
  ground_station.py send raw MID CC [HEXPAYLOAD]           any command, e.g. for error tests
  ground_station.py interactive [--log FILE]               listen and type commands at the same time
Common options: --host (command destination), --cmd-port, --tlm-port.

Formats (must match the flight software; the automated test scripts/12c-run-ground-station-sim.sh checks them):
  command:   CCSDS primary header (6 bytes, big-endian: stream id = MID, sequence 0xC000, length = total - 7)
             + cFS command secondary header (function code, checksum) + payload (little-endian on the
             simulator). Checksum: 0xFF XOR all packet bytes, computed with the checksum byte at 0
             (cFE CFE_MSG_ComputeCheckSum).
  telemetry: 16-byte header (CCSDS primary 6, time 6, spare 4) + payload.
  counter_app (apps/counter_app/fsw/inc/counter_app_msgids.h, counter_app_msg.h): commands MID 0x1890 with
             NOOP 0, RESET_COUNTERS 1, SET_PERIOD 2 (uint32 PeriodMs); housekeeping MID 0x0890, payload
             Counter, PeriodMs (uint32), LibCallStatus (int32), CommandCounter, CommandErrorCounter,
             FileTestFailedStep, Spare (uint8).
  cFE ES commands MID 0x1806 (NOOP 0); to_lab commands MID 0x1880 (OUTPUT_ENABLE 6, char dest_IP[16]);
  EVS long event message MID 0x0808, short 0x0809.
No authentication: ci_lab accepts commands from any UDP sender (NASA lab apps); localhost simulator use only.
"""
import argparse
import socket
import struct
import sys
import threading
import time

COUNTER_CMD_MID = 0x1890
COUNTER_HK_MID = 0x0890
ES_CMD_MID = 0x1806
TO_LAB_CMD_MID = 0x1880
EVS_LONG_MID = 0x0808
EVS_SHORT_MID = 0x0809
TLM_HDR = 16
HK = struct.Struct("<IIiBBBB")
EVENT_TYPES = {1: "DEBUG", 2: "INFO", 3: "ERROR", 4: "CRITICAL"}


# ---- commands ---------------------------------------------------------------------------------------------

def build_command(mid, cc, payload=b""):
    """cFS v1 command packet: CCSDS primary header, function code, checksum, payload."""
    length = 6 + 2 + len(payload)
    pkt = bytearray(struct.pack(">HHH", mid, 0xC000, length - 7))
    pkt += bytes([cc & 0x7F, 0]) + payload
    chk = 0xFF
    for b in pkt:
        chk ^= b
    pkt[7] = chk
    return bytes(pkt)


def command_from_args(words):
    """Turns e.g. ['set-period', '2000'] into a packet; raises ValueError with a usage text."""
    if not words:
        raise ValueError("no command")
    name, args = words[0], words[1:]
    if name == "tlm-enable":
        ip = args[0] if args else "127.0.0.1"
        if len(ip) > 15:
            raise ValueError("IP address too long")
        return build_command(TO_LAB_CMD_MID, 6, ip.encode("ascii").ljust(16, b"\0"))
    if name == "noop" and not args:
        return build_command(COUNTER_CMD_MID, 0)
    if name == "reset" and not args:
        return build_command(COUNTER_CMD_MID, 1)
    if name == "set-period" and len(args) == 1:
        return build_command(COUNTER_CMD_MID, 2, struct.pack("<I", int(args[0], 0)))
    if name == "es-noop" and not args:
        return build_command(ES_CMD_MID, 0)
    if name == "raw" and len(args) in (2, 3):
        payload = bytes.fromhex(args[2]) if len(args) == 3 else b""
        return build_command(int(args[0], 0), int(args[1], 0), payload)
    raise ValueError("commands: tlm-enable [IP] | noop | reset | set-period MS | es-noop | raw MID CC [HEX]")


def send(packet, host, port):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(packet, (host, port))


# ---- telemetry --------------------------------------------------------------------------------------------

def decode(pkt):
    """One text line for a telemetry packet."""
    if len(pkt) < 6:
        return "SHORT LEN=%d" % len(pkt)
    mid = struct.unpack(">H", pkt[0:2])[0]
    head = "MID=0x%04X LEN=%d" % (mid, len(pkt))
    body = pkt[TLM_HDR:]
    if mid == COUNTER_HK_MID and len(body) == HK.size:
        c, period, lib, cmd, err, ftest, _ = HK.unpack(body)
        return head + " COUNTER_HK counter=%d period=%d lib=%d cmdcnt=%d errcnt=%d filetest=%d" % (
            c, period, lib, cmd, err, ftest)
    if mid in (EVS_LONG_MID, EVS_SHORT_MID) and len(body) >= 32:
        app = body[0:20].split(b"\0")[0].decode("ascii", "replace")
        eid, etype = struct.unpack("<HH", body[20:24])
        text = body[32:].split(b"\0")[0].decode("ascii", "replace") if mid == EVS_LONG_MID else ""
        return head + " EVENT %s %d %s: %s" % (app, eid, EVENT_TYPES.get(etype, str(etype)), text)
    return head


def listen(port, seconds, log, stop=None):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("0.0.0.0", port))
        s.settimeout(0.2)
        end = None if seconds is None else time.monotonic() + seconds
        line = "LISTEN %.6f port %d" % (time.time(), port)
        emit(line, log)
        while (end is None or time.monotonic() < end) and not (stop and stop.is_set()):
            try:
                pkt, _ = s.recvfrom(65536)
            except socket.timeout:
                continue
            emit("%.6f %s" % (time.time(), decode(pkt)), log)


def emit(line, log):
    print(line, flush=True)
    if log:
        log.write(line + "\n")
        log.flush()


# ---- main ---------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="m2s-cfs ground station (simulator)")
    ap.add_argument("--host", default="127.0.0.1", help="command destination (ci_lab)")
    ap.add_argument("--cmd-port", type=int, default=1234)
    ap.add_argument("--tlm-port", type=int, default=1235)
    sub = ap.add_subparsers(dest="mode", required=True)
    p = sub.add_parser("listen")
    p.add_argument("--seconds", type=float)
    p.add_argument("--log")
    p = sub.add_parser("send")
    p.add_argument("words", nargs="+")
    p.add_argument("--print-bytes", action="store_true", help="also print the packet in hex")
    p = sub.add_parser("interactive")
    p.add_argument("--log")
    a = ap.parse_args()

    if a.mode == "send":
        try:
            pkt = command_from_args(a.words)
        except ValueError as e:
            print("error: %s" % e, file=sys.stderr)
            return 2
        send(pkt, a.host, a.cmd_port)
        if a.print_bytes:
            print(pkt.hex())
        return 0

    log = open(a.log, "a") if a.log else None
    try:
        if a.mode == "listen":
            listen(a.tlm_port, a.seconds, log)
            return 0
        stop = threading.Event()
        t = threading.Thread(target=listen, args=(a.tlm_port, None, log, stop), daemon=True)
        t.start()
        print("commands: tlm-enable [IP] | noop | reset | set-period MS | es-noop | raw MID CC [HEX] | quit")
        for line in sys.stdin:
            words = line.split()
            if not words:
                continue
            if words[0] in ("quit", "exit"):
                break
            try:
                send(command_from_args(words), a.host, a.cmd_port)
                emit("%.6f SENT %s" % (time.time(), " ".join(words)), log)
            except ValueError as e:
                print("error: %s" % e)
        stop.set()
        t.join(1)
        return 0
    finally:
        if log:
            log.close()


if __name__ == "__main__":
    sys.exit(main())
