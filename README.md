# sferic

MIT-licensed ESP32-S3 hardware / radio multitool firmware for the **M5Stack
StickS3**. Bruce is a design reference, not a fork ([ADR-0001](docs/adr/0001-onboard-rf-and-tool-capabilities.md)).

A sferic (short for atmospheric radio wave) is a natural radio frequency emission caused by lightning.

The architecture is a **lightning-detection network**: distributed
*stations* (M5 devices) capture RF impulses; a central collector correlates
them. Vocabulary follows the field — station, **strike** (one event),
**flash** (a capture session). See
[`docs/adr/`](docs/adr/) for the full design and [`docs/mcp/`](docs/mcp/)
for the per-toolset contracts.

## Build & flash

```sh
pio run -e sferic                    # default build
pio run -e sferic -t upload          # flash (StickS3 on USB-C)
pio device monitor -b 115200         # serial — assert DTR
```

Toolchain: PlatformIO + Arduino framework + M5Unified. (ESP-IDF is a dead
end here — M5GFX crashes in its SPI init under IDF 5.3.)

**`sferic-usbhid`** is a second build ([ADR-0005](docs/adr/0005-usb-mode-and-usb-hid-build.md)):
`ARDUINO_USB_MODE=0` (TinyUSB), which unlocks the USB-HID tool but makes
flashing less reliable — hold the side button while connecting to force the
bootloader. The default `sferic` build keeps the rock-solid
USB-Serial/JTAG.

**`sferic-rfpack`** adds the `subghz` (CC1101) + `nrf24` (nRF24L01+) toolsets
for the pingequa RF Pack S3 backpack
([ADR-0012](docs/adr/0012-rf-pack-spi-arbitration.md),
[pin map](docs/hardware/rf-pack-s3.md)). Drivers are scaffolded from the
datasheets and unverified — the build boots and reports "RF Pack not
detected" with no board attached. Same USB mode as `sferic`.

**`sferic-ws`** is the networked station
([ADR-0009](docs/adr/0009-station-collector-transport.md),
[ws-transport](docs/mcp/ws-transport.md)): it opens a persistent WebSocket to
the Pi collector and speaks the same op vocabulary as the serial link
(`src/dispatch.*` is shared). Needs `src/ws_config.h` — copy
`src/ws_config.h.example` and fill in the Wi-Fi creds, collector address, and
per-station token (gitignored). The collector-side endpoint is not built
yet.

## Controls

Two buttons: **A** = next / (hold) prev · **B** = select / (hold) back.
Over serial you can also type `a A b B` to drive the menu.

Tools that emit IR / RF / keystrokes (TV-B-Gone, IR Replay, BLE/USB HID) are
**two-step**: B arms, B again fires, A cancels, 3-second timeout.

## Tools

| Toolset | Tools |
|---|---|
| **WiFi** | AP Scan · Packet Monitor · Probe/Beacon · Evil Portal · Handshake Capture · Wardrive · Deauth Detector |
| **Bluetooth** | Scan (→ decode → GATT browse) · Spam Detect · HID Keyboard |
| **Infrared** | Learn / Replay · TV-B-Gone (~39 codes) · Protocol Decode |
| **Sensors** | Motion / Tamper · Mic Spectrum · Gesture |
| **USB** *(sferic-usbhid only)* | HID Keyboard |
| **SubGHz / 2.4GHz** *(sferic-rfpack only)* | Band Scan ×2 — scaffolded, RF Pack S3 not yet verified |
| **Settings** | Tools (enable/disable, persisted) · Display (sleep timeout + brightness) · Capture (pcap buffer) · Web UI · About |

Each tool is a `registry.add({...})` line in `src/menu_tree.cpp` plus a
`Screen`; the menu is generated from the registry, filtered by the
persisted enable flags. Authorized-use scope is in
[ADR-0001](docs/adr/0001-onboard-rf-and-tool-capabilities.md) — capture /
replay / decode / scan, no untargeted-DoS headline features.

## Web UI

`Settings > Web UI` stands up an open SoftAP `sferic-ui` (`192.168.4.1`) and
a one-page dashboard: tool enable/disable, a **Run** button on
`run()`-capable tools, live status, screen sleep / brightness, and a
`capture.pcap` download. It is the exclusive WiFi user while up — WiFi tools
pause. ([ADR-0006](docs/adr/0006-web-ui.md))

The page is authored in [`web/`](web/) (`index.html` + `style.css` +
`app.js`); `scripts/gen_webui.py` inlines it into `src/webui_html.h` on
every firmware build (CI checks the committed header is current) — don't
hand-edit that header.

`GET /api/tools/run?id=<tool-id>` is the headless launch path — the same
`Registry::run` dispatch the MCP server will use. Only non-radio tools
(`sensors.motion`, `sensors.gesture`) expose it on the station for now;
`/api/tools` reports `has_run` per tool. ([`docs/mcp/headless.md`](docs/mcp/headless.md))

## Collector link

A tethered station also answers a headless command channel over USB serial —
`>{"op":"..."}` lines in, `<{...}` lines out, coexisting with log output.
`hello` (capability declaration), `status`, `tools`, `run` (same
`Registry::run` dispatch), `set`. This is the milestone-2 transport for
[`m5stack-lab/sferic-collector`](https://github.com/m5stack-lab/sferic-collector); over
serial the radio is free, so every toolset is reachable once it has a
headless path. ([`docs/mcp/serial-transport.md`](docs/mcp/serial-transport.md))

The collector also auto-enriches `wifi.scan`/`ble.scan` results with a
best-effort vendor + known-CVE lead per device (OUI lookup + cve-search) —
see [ADR-0010](docs/adr/0010-cve-enrichment.md).

## Captures

No SD slot, so 802.11 captures go to a **512 KB PSRAM pcap buffer**
(`LINKTYPE_IEEE802_11_RADIOTAP`, valid Wireshark files). Export: the web
UI's `capture.pcap` download, or base64 over serial from `Settings >
Capture`. The fleet model ([ADR-0003](docs/adr/0003-companion-mcp-server.md),
[ADR-0007](docs/adr/0007-fleet-composition.md)) hands captures to the MCP
collector — no local persistence needed.

## Layout

```
src/hal.*        M5Unified wrapper — the only board-specific code
src/ui.*         Screen + ScreenStack, MenuScreen, InfoScreen
src/registry.*   tool registry + NVS-persisted enable flags
src/menu_tree.*  registerBuiltinTools() + the live menu
src/pcap.*       PSRAM pcap capture buffer
src/confirm.h    two-step arm/confirm guard
src/serial_control.*  headless command channel (collector transport)
src/<tool>.*     one screen per tool
web/             web UI sources -> src/webui_html.h via scripts/gen_webui.py
docs/adr/        architecture decision records (0001–0011)
docs/mcp/        per-toolset MCP contracts + strike/pcap formats
test/            host unit tests — `pio test -e native`
```

## Status

StickS3 firmware + local web UI, with screen sleep and the headless
`run()` seam in place (sensors only so far — radio tools get their headless
path on the collector). Not built yet: the MCP server (Pi 5 collector),
RF Pack S3 drivers (SubGHz / nRF24 — waiting on hardware).

## Releases

Versioned by `SFERIC_VERSION` in `src/version.h`. Pushing a `vX.Y.Z` tag on
`main` runs CI for both envs and publishes a [GitHub
Release](https://github.com/m5stack-lab/sferic-sticks3/releases) with `sferic-<tag>.bin`
(default USB-Serial/JTAG build) and `sferic-usbhid-<tag>.bin` (TinyUSB
HID build) attached, plus auto-generated notes.

```sh
git tag -a v0.1.0 -m "sferic v0.1.0"
git push origin v0.1.0
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Short version: clean-room only (no
Bruce/GPL code), design changes carry an ADR, branch + PR off `main`, keep
both `pio run` envs green, no secrets.

## Contributors

- **[ry-ops](https://github.com/ry-ops)** — author / maintainer

Firmware co-authored with [Claude Code](https://claude.com/claude-code)
(trailers on the commits).

## License

[MIT](LICENSE) © 2026 ry-ops
