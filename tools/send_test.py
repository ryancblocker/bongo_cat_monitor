#!/usr/bin/env python3
"""Send sample data to the Bongo Cat board without the desktop app.

    python3 tools/send_test.py /dev/cu.usbserial-10     (Mac)
    python  tools/send_test.py COM3                     (Windows)

Needs pyserial (pip install pyserial). Opening the port restarts the board,
so the script waits for it to boot, then fills every row and types for 10 s.
"""
import sys, time
import serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbserial-10"
s = serial.Serial(port, 115200, timeout=0.2)
time.sleep(3)  # board reboots when the port opens
s.reset_input_buffer()

def send(cmd):
    s.write((cmd + "\n").encode())
    time.sleep(0.06)

send("PING")
send("TIME_FORMAT:12")
send("TIME:21:41")
send("DATE:THU 12")
send("WX:54F|CLOUDY")
send("HW:CPUT:136,GPU:63,GPUT:160,U:F")
end = time.time() + 10
while time.time() < end:
    send("STATS:CPU:45,RAM:67,WPM:82")
    send("SPEED:150")
    time.sleep(0.5)
send("STOP")
out = s.read(4096).decode(errors="replace")
print("Board replied PONG" if "PONG" in out else "No PONG seen (check the port)")
