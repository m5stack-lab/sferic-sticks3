# Capture buffer & pcap export

**Status:** draft — living spec (ADR-0003 §7).
**Firmware:** `src/pcap.{h,cpp}` (`PcapBuffer capture` — one shared buffer),
`src/capture_screen.cpp` (Settings > Capture), consumed by
`src/wifi_handshake.cpp` and future WiFi capture tools.

## Buffer

The StickS3 has no SD card, so 802.11 captures go to a single **512 KB
PSRAM ring** (`capture`). It holds a standard **pcap** stream:

- global header: magic `a1b2c3d4`, version 2.4, snaplen 65535,
  network **127 = `LINKTYPE_IEEE802_11_RADIOTAP`**
- per frame: 16-byte pcap record header + a 15-byte radiotap header
  (`RATE | CHANNEL | DBM_ANTSIGNAL`) + the raw 802.11 frame

Wireshark, `tshark`, and aircrack-ng read it as-is. When the buffer fills it
stops recording and flags `full` (no wrap — a partial capture is still a
valid pcap).

## Export / hand-off

Today: **base64 over serial**, framed so a host can pull it out:

```
---PCAP-BEGIN <bytes> <frames>---
<base64, 76 cols>
---PCAP-END---
```

Decode: strip the markers, `base64 -d > capture.pcap`.

Planned (ADR-0003): the station hands the buffer to the **MCP server** or a
peer device over the station link — `sferic.capture_pull { station }` →
the pcap bytes, or a push when a capture session (flash) closes. The StickS3
never needs local persistence; the collector owns storage.

## MCP surface (planned)

- `sferic.capture_status { station }` → `{ frames, used, capacity, recording, full }`
- `sferic.capture_pull { station, format: "pcap" }` → the bytes (base64 or binary)
- `sferic.capture_clear { station }`
