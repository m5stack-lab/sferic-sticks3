"""Verify the screen still sleeps while the Web UI SoftAP is up, and that
sleep gates *only* the backlight — the AP and HTTP server keep serving.

    python scripts/dev/check_ap_sleep.py

Menu path assumed: root -> Settings (index 4) -> Web UI (index 3). If the
menu layout changes, the '[webui] up' wait fails fast with a hint.
"""

import sys

from serial_console import Console


def main():
    with Console() as c:
        c.to_root()
        c.send("aaaa")
        c.send("b", gap=0.8)          # -> Settings
        c.send("aaa")
        c.send("b", gap=1.5)          # -> Web UI (starts the SoftAP)

        if not c.wait_for("[webui] up", timeout=6):
            sys.exit("FAIL: Web UI did not start (menu indices changed?)")

        print(">>> AP up; idling 42s (expect sleep ~30s, AP stays up)", flush=True)
        idle = c.read_lines(42, tag="idle ")
        if not any("screen sleep" in l for l in idle):
            sys.exit("FAIL: screen never slept with the AP up")
        if len([l for l in idle if "awake=0" in l and "[beat]" in l]) < 2:
            sys.exit("FAIL: no beats after sleep — server/loop stalled")

        print(">>> waking + leaving", flush=True)
        c.send("a")
        if not c.wait_for("screen wake", timeout=5):
            sys.exit("FAIL: did not wake")
        c.send("B", gap=1.0)
        c.read_lines(2, tag="exit ")
        print("PASS: screen slept, AP + server kept ticking, woke clean")


if __name__ == "__main__":
    main()
