"""Serial helpers for desk-testing sferic on the StickS3.

The StickS3's USB-Serial/JTAG only transmits when the host holds the port
open with DTR asserted, so every connection here sets ``dtr = True``.
Re-flashing (``pio run -e sferic -t upload``) is still the only reliable
hardware reset.

Standalone monitor::

    python scripts/dev/serial_console.py [seconds] [--grep REGEX]

As a module::

    from serial_console import Console
    with Console() as c:
        c.to_root()
        c.send("aaab")          # menu nav: a/A = next/prev, b/B = select/back
        c.read_lines(5)

Needs ``pyserial`` (``pip install pyserial``).
"""

import re
import sys
import time

import serial

PORT = "/dev/cu.usbmodem1101"
BAUD = 115200


class Console:
    def __init__(self, port=PORT, baud=BAUD):
        self.s = serial.Serial(port, baud, timeout=0.5)
        self.s.dtr = True
        time.sleep(0.4)
        self.s.reset_input_buffer()
        self.t0 = time.time()

    def close(self):
        self.s.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def send(self, tokens, gap=0.55):
        """Send menu-nav chars one at a time (a burst collapses on-device if
        sent together): 'a'/'A' = next/prev, 'b'/'B' = select/back."""
        for ch in tokens:
            self.s.write(ch.encode())
            self.s.flush()
            time.sleep(gap)

    def to_root(self, n=4):
        """Back out to the root menu from anywhere — B at root is harmless."""
        self.send("B" * n, gap=0.35)

    def read_lines(self, seconds, tag="", echo=True):
        """Collect non-empty lines for `seconds`, echoing each with a
        relative timestamp. Returns the list."""
        out, end = [], time.time() + seconds
        while time.time() < end:
            ln = self.s.readline().decode(errors="replace").rstrip()
            if not ln:
                continue
            out.append(ln)
            if echo:
                print(f"{time.time() - self.t0:7.1f}  {tag}{ln}", flush=True)
        return out

    def wait_for(self, needle, timeout=20):
        """Block until a line contains `needle`; return it, or None on timeout."""
        end = time.time() + timeout
        while time.time() < end:
            ln = self.s.readline().decode(errors="replace").rstrip()
            if ln:
                print(f"{time.time() - self.t0:7.1f}  {ln}", flush=True)
                if needle in ln:
                    return ln
        return None


def _main(argv):
    secs, grep = 30.0, None
    i = 0
    while i < len(argv):
        if argv[i] == "--grep":
            grep = re.compile(argv[i + 1])
            i += 2
        else:
            secs = float(argv[i])
            i += 1
    with Console() as c:
        for ln in c.read_lines(secs, echo=(grep is None)):
            if grep and grep.search(ln):
                print(ln, flush=True)


if __name__ == "__main__":
    _main(sys.argv[1:])
