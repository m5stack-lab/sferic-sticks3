# MCP contract: `usb` toolset

**Status:** draft — living spec (ADR-0003 §7).
**Firmware:** `src/usb_hid.cpp` — **only in the `sferic-usbhid` build**
(`ARDUINO_USB_MODE=0`, `-DSFERIC_USB_HID`; see ADR-0005). The default
`sferic` firmware has no `usb` toolset.
**Toolset id:** `usb`

A station declares the `usb` capability only when running the `sferic-usbhid`
firmware. On other builds `usb.*` is `capability_not_present`.

---

## `usb.hid`  — implemented (firmware: USB HID Keyboard)

Enumerates as a USB keyboard to the host the StickS3 is plugged into.
`{ station, text }` → types `text` as key events (boot-keyboard reports,
ascii subset: letters, digits, space, newline, `.-_`). Authorized input
testing only (ADR-0001).
