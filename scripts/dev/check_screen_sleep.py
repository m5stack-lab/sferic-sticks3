"""Verify screen sleep end to end over serial: the backlight goes off after
the idle timeout, the firmware keeps ticking, and any input wakes it.

    python scripts/dev/check_screen_sleep.py

Reads the [hal] / [beat] logs — there is nothing on-screen to assert here.
Assumes the default 30s timeout (Settings > Display / NVS).
"""

import sys

from serial_console import Console


def main():
    with Console() as c:
        c.to_root()
        print(">>> idle; waiting for [hal] screen sleep (<= 45s)...", flush=True)
        if not c.wait_for("screen sleep", timeout=45):
            sys.exit("FAIL: never slept")

        asleep = c.read_lines(5, tag="asleep ")
        if not any("awake=0" in l for l in asleep):
            sys.exit("FAIL: still reporting awake after sleep")
        if len([l for l in asleep if "[beat]" in l]) < 2:
            sys.exit("FAIL: beats stopped — loop stalled while asleep")

        print(">>> sending 'a' to wake", flush=True)
        c.send("a")
        if not c.wait_for("screen wake", timeout=5):
            sys.exit("FAIL: did not wake on input")
        if not any("awake=1" in l for l in c.read_lines(4, tag="awake ")):
            sys.exit("FAIL: awake flag not set after wake")

        print("PASS: slept at the timeout, kept running, woke on input")


if __name__ == "__main__":
    main()
