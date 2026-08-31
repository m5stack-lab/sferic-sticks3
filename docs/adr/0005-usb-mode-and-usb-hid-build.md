# ADR-0005 — USB mode, and the USB-HID build variant

**Status:** Accepted
**Date:** 2026-08-30
**Relates to:** ADR-0002 (toolchain), ADR-0003 (capability declaration),
ADR-0004 (tool registry)

## Context

The ESP32-S3's USB peripheral has two mutually-exclusive Arduino modes,
fixed at **compile time**:

- **`ARDUINO_USB_MODE=1`** — hardware USB-Serial/JTAG (HWCDC). Rock-solid for
  `esptool` flashing and `pio device monitor`. This is what the project uses
  today (`platformio.ini`). Cannot present a USB HID interface.
- **`ARDUINO_USB_MODE=0`** — TinyUSB / USB-OTG. Can enumerate a **composite**
  device: CDC serial *and* HID. Enables BadUSB-style keystroke injection.
  Cost: flashing / monitor are less reliable — the board often needs a
  manual bootloader entry (hold the side button on connect), and TinyUSB CDC
  drops more readily than HWCDC.

USB HID (`usb.hid` in the ADR-0001 catalog) fundamentally repurposes the USB
port. It is one tool; flashing reliability matters on every build.

## Decision

Keep the **default build on `ARDUINO_USB_MODE=1`**. Add a separate
PlatformIO env, **`sferic-usbhid`**, that sets `ARDUINO_USB_MODE=0` plus
`-DSFERIC_USB_HID`.

- `usb.*` tools are compiled and registered **only** under `-DSFERIC_USB_HID`.
  The default firmware has no USB toolset in the menu.
- A station declares the `usb` capability (ADR-0003 §3) based on which
  firmware it is running — the MCP server / web UI show `usb.hid` only for
  stations on the `sferic-usbhid` build.
- Serial on `sferic-usbhid` is TinyUSB CDC (still works, just via a
  different endpoint). Flashing that build may require holding the side
  button on connect.

## Consequences

- Two firmware variants: `sferic` (everything except USB HID, reliable
  flashing) and `sferic-usbhid` (adds USB HID, trickier flashing).
- `registerBuiltinTools()` guards the USB tool with `#ifdef SFERIC_USB_HID`.
- `platformio.ini` gains `[env:sferic-usbhid]` extending the firmware base
  with the two flags.
- Build/flash docs note the manual-bootloader step for the usbhid variant.
- If a future board (Cardputer, etc.) has a separate always-on debug UART,
  it could run USB HID *and* keep reliable serial — that's a per-board
  call, not a change here.

## Alternatives considered

- **Switch the whole project to `ARDUINO_USB_MODE=0`.** Rejected: makes
  every flash of every build less reliable for the sake of one tool.
- **Runtime toggle.** Not possible — USB mode is compile-time.
- **Drop USB HID.** Rejected: keystroke injection is a core multitool
  capability; a separate build is a fair price.
