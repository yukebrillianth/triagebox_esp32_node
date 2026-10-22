#!/usr/bin/env python3
"""
Capture one BP measurement window from the node as CSV, raw and filtered.

    tools/bp_capture_csv.py              # record mode: no patient, no measurement
    tools/bp_capture_csv.py --measure    # wait for a real 60 s measurement
    tools/bp_capture_csv.py 30 out.csv   # 30 s recording to out.csv

Record mode (default) is the one to use for signal analysis: it starts
capturing immediately -- attach the sensors and hold still -- with no card, no
screen flow and no BP published. --measure arms the dump and waits for a full
measurement instead; use it when you want the CSV to carry a real prediction.

Output is a plain CSV with `#` comment lines carrying the prediction and the
filter coefficients that produced it, so the file still says what it is weeks
later. Read it with pandas.read_csv(path, comment='#').

Only one process may hold the port: stop any monitor/recorder first.
"""
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing -- run this with "
             "~/.espressif/python_env/*/bin/python")

args = [a for a in sys.argv[1:] if not a.startswith("--")]
measure = "--measure" in sys.argv
SECONDS = int(args[0]) if args and args[0].isdigit() else 60
PORT = args[1] if len(args) > 1 else "/dev/cu.usbmodem1401"
OUT = args[2] if len(args) > 2 else "bp_window.csv"
TIMEOUT_S = SECONDS * 2 + 300

# A data row: seven fields, i + three integers (ECG signed) then three decimals.
# Anchored on shape rather than block markers because ESP_LOG lines interleave
# with the dump -- the poll task keeps logging while it prints.
ROW = re.compile(r"^\d+,\d+,\d+,-?\d+,-?[\d.]+,-?[\d.]+,-?[\d.]+$")
HEADER = "i,raw_ir,raw_red,raw_ecg,fil_ir,fil_red,fil_ecg"

with serial.Serial(PORT, 115200, timeout=2) as port, open(OUT, "w") as out:
    port.reset_input_buffer()
    port.write(b"bplog\n" if measure else f"bplog {SECONDS}\n".encode())
    if measure:
        print(f"armed on {PORT}; run one full measurement now (writing {OUT})")
    else:
        print(f"recording {SECONDS}s on {PORT} -- attach sensors, hold still "
              f"(writing {OUT})")

    rows = 0
    deadline = time.time() + TIMEOUT_S
    while time.time() < deadline:
        line = port.readline().decode(errors="replace").strip()
        if not line:
            continue
        if line.startswith("# triagebox bp window") or line.startswith("# hp05"):
            out.write(line + "\n")
            out.flush()
            print(line)
        elif line == HEADER:
            out.write(line + "\n")
        elif ROW.match(line):
            out.write(line + "\n")
            rows += 1
            if rows % 1000 == 0:
                print(f"  {rows} samples...")
        elif line.startswith("# end"):
            break
        elif "BP " in line or "recording" in line:
            print(line)  # the verdict, or the record-mode banner

if rows == 0:
    sys.exit("no samples captured -- was the window run at all?")
print(f"{rows} samples -> {OUT}")
