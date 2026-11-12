#!/usr/bin/env python3
"""Drive the Bongo Cat board from a computer without the desktop app (close the app first:
only one program can hold the port).

    .venv/bin/python tools/bongo.py demo                      # sample weather + PC data, cat types
    .venv/bin/python tools/bongo.py demo --code 61 --night    # rain at night (WMO weather code)
    .venv/bin/python tools/bongo.py demo --shot screen.png    # ...then save a screenshot
    .venv/bin/python tools/bongo.py send "SET:coat=grey|name=Mochi" --shot s.png
    .venv/bin/python tools/bongo.py shot screen.png

Opening the port restarts the board, so each run sends everything it needs.
Options: --port (default: first CH340/CP210x port), --wait seconds before the screenshot.
"""
import argparse, glob, sys, time
import serial

W, H = 240, 320


def find_port():
    for pat in ("/dev/cu.usbserial*", "/dev/cu.wchusbserial*", "/dev/cu.SLAB*", "/dev/ttyUSB*", "COM*"):
        hits = sorted(glob.glob(pat))
        if hits:
            return hits[0]
    sys.exit("No board found; pass --port")


def open_board(port):
    s = serial.Serial(port, 115200, timeout=0.2)
    s.dtr = False  # reset the board (EN via RTS), like esptool's hard reset
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    t = time.time()
    buf = b""
    while time.time() - t < 6:  # wait for the boot banner
        buf += s.read(256)
        if b"HELLO:bongo2" in buf:
            break
    if b"HELLO:bongo2" not in buf:
        print("warning: no HELLO from the board (old firmware, or waiting on touch calibration?)")
    return s


def send(s, line):
    s.write((line + "\n").encode())
    time.sleep(0.05)


def demo(s, a):
    hhmm = time.strftime("%H:%M:%S")
    send(s, f"TIME:{hhmm}")
    send(s, "DATE:" + time.strftime("%a, %b ") + str(int(time.strftime("%d"))))
    send(s, f"WX2:code={a.code}|day={0 if a.night else 1}|t=54|hi=60|lo=41|wind=8|wu=mph|hum=40|u=F|place=Highlands Ranch")
    gpu = "|gpu=63|gput=160" if not a.no_gpu else ""
    send(s, f"PC:cpu=23|ram=41|wpm=82|cput=136{gpu}|u=F")
    send(s, "URL:http://192.168.1.20:8787/c/demo42/")
    if a.typing:
        send(s, "STREAK_ON")
        end = time.time() + a.typing
        while time.time() < end:
            send(s, "SPEED:90")
            time.sleep(0.25)


def shot(s, path):
    s.reset_input_buffer()
    send(s, "SHOT")
    s.timeout = 30
    hdr = b""
    while not hdr.endswith(b"SHOT:240x320\n"):
        c = s.read(1)
        if not c:
            sys.exit("no screenshot header")
        hdr = (hdr + c)[-64:]
    raw = b""
    while len(raw) < W * H * 2:
        chunk = s.read(W * H * 2 - len(raw))
        if not chunk:
            sys.exit(f"screenshot cut short ({len(raw)} bytes)")
        raw += chunk
    from PIL import Image
    img = Image.new("RGB", (W, H))
    px = img.load()
    for i in range(W * H):
        v = (raw[2 * i] << 8) | raw[2 * i + 1]  # stored big-endian
        px[i % W, i // W] = (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
    img.resize((W * 2, H * 2), Image.NEAREST).save(path)
    print("saved", path)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["demo", "send", "shot", "dbg"])
    ap.add_argument("arg", nargs="*")
    ap.add_argument("--port")
    ap.add_argument("--code", type=int, default=3, help="WMO weather code (0 clear, 2 partly, 3 cloudy, 45 fog, 61 rain, 71 snow, 95 storm)")
    ap.add_argument("--night", action="store_true")
    ap.add_argument("--no-gpu", action="store_true")
    ap.add_argument("--typing", type=float, default=0, help="seconds of fast typing")
    ap.add_argument("--wait", type=float, default=1.0)
    ap.add_argument("--shot")
    a = ap.parse_args()
    s = open_board(a.port or find_port())
    if a.cmd == "demo":
        demo(s, a)
    elif a.cmd == "send":
        demo(s, a)
        for line in a.arg:
            send(s, line)
    elif a.cmd == "shot":
        demo(s, a)
        a.shot = a.arg[0] if a.arg else "screen.png"
    if a.cmd == "dbg":
        demo(s, a)
        s.reset_input_buffer()
        send(s, "DBG")
        time.sleep(0.4)
        print(s.read(2000).decode(errors="replace"))
    if a.shot:
        time.sleep(a.wait)
        shot(s, a.shot)
