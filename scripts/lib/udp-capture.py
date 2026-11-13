#!/usr/bin/env python3
"""udp-capture.py — host-side telemetry capture for scripts/12b-run-tctm-sim.sh (step 12b, D42).

Listens on a UDP port for a fixed time and prints one line per datagram:
    <host time> MID=0x0890 LEN=32 [decoded fields]
counter_app housekeeping (MID 0x0890) is decoded from the LAST 16 bytes (payload of counter_app_msg.h,
little-endian: Counter, PeriodMs, LibCallStatus as uint32/int32, then CommandCounter, CommandErrorCounter,
FileTestFailedStep, Spare as uint8), so it does not depend on the telemetry header size, which is printed.
EVS long event messages (MID 0x0808) show the app name and the event text.
Python 3 standard library only. Exit status 0, or 1 if the port cannot be bound.
"""
import argparse
import socket
import struct
import sys
import time

HK_MID = 0x0890
EVS_LONG_MID = 0x0808
HK_PAYLOAD = struct.Struct("<IIiBBBB")  # 16 bytes


def ccsds_mid(pkt):
    return struct.unpack(">H", pkt[0:2])[0]


def describe(pkt):
    mid = ccsds_mid(pkt)
    text = "MID=0x%04X LEN=%d" % (mid, len(pkt))
    if mid == HK_MID and len(pkt) >= HK_PAYLOAD.size + 6:
        c, period, lib, cmd, err, ftest, _ = HK_PAYLOAD.unpack(pkt[-HK_PAYLOAD.size:])
        text += " HDR=%d counter=%d period=%d lib=%d cmdcnt=%d errcnt=%d filetest=%d" % (
            len(pkt) - HK_PAYLOAD.size, c, period, lib, cmd, err, ftest)
    elif mid == EVS_LONG_MID:
        # After the telemetry header: AppName[20], EventID u16, EventType u16, SpacecraftID u32,
        # ProcessorID u32, Message[...]. The header size is the one seen in housekeeping (normally 16).
        for hdr in (16, 12):
            name = pkt[hdr:hdr + 20].split(b"\0")[0]
            if name and all(32 <= b < 127 for b in name):
                eid = struct.unpack("<H", pkt[hdr + 20:hdr + 22])[0]
                msg = pkt[hdr + 32:].split(b"\0")[0].decode("ascii", "replace")
                text += " EVENT %s %d: %s" % (name.decode(), eid, msg)
                break
    return text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=1235)
    ap.add_argument("--seconds", type=float, required=True)
    args = ap.parse_args()

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.bind(("127.0.0.1", args.port))
    except OSError as e:
        print("CAPTURE ERROR: cannot bind 127.0.0.1:%d: %s" % (args.port, e), flush=True)
        return 1
    print("CAPTURE START %.6f port %d" % (time.time(), args.port), flush=True)
    end = time.monotonic() + args.seconds
    while True:
        left = end - time.monotonic()
        if left <= 0:
            break
        s.settimeout(left)
        try:
            pkt, _ = s.recvfrom(65536)
        except socket.timeout:
            break
        print("%.6f %s" % (time.time(), describe(pkt)), flush=True)
    print("CAPTURE END %.6f" % time.time(), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
