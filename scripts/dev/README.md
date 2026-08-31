# scripts/dev — hardware-in-the-loop helpers

Desk-testing tools for a StickS3 on USB. Not part of the build (the firmware
`extra_scripts` are `../git_rev.py` and `../gen_webui.py`).

```sh
pip install pyserial
python scripts/dev/serial_console.py 20            # 20s timestamped monitor
python scripts/dev/serial_console.py 60 --grep hal # filter
python scripts/dev/check_screen_sleep.py           # sleep -> keeps running -> wake
python scripts/dev/check_ap_sleep.py               # screen sleeps, Web UI AP stays up
```

`serial_console.Console` wraps the two StickS3 gotchas: the USB-Serial/JTAG
port only transmits with **DTR asserted**, and menu-nav chars must be sent
**one at a time** (`a`/`A` = next/prev, `b`/`B` = select/back) or they
collapse on-device. Port defaults to `/dev/cu.usbmodem1101`.

The `check_*` scripts assert on the `[hal]` / `[beat]` serial logs and exit
non-zero on failure. `check_ap_sleep.py` drives the menu by index, so it
depends on the current layout (root → Settings → Web UI).
