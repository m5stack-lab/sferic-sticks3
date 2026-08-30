# ADR-0001 — Onboard + RF Pack hardware capabilities and the tool catalog

**Status:** Accepted
**Date:** 2026-08-30
**Deciders:** ry-ops (operator)

## Context

The project is a from-scratch, MIT-licensed ESP32-S3 firmware for the
**M5Stack StickS3** — a menu-driven hardware / radio multitool in the spirit
of pr3y/Bruce (reference only, not a fork; Bruce is AGPL). The pingequa
**RF Pack S3** backpack (nRF24L01+ + CC1101) is a first-class add-on.

We need one place that states what the hardware can physically do and what
tools we intend to build on it, so feature work has a fixed target and a
priority order.

## Decision

Target the capability set below. Tools are built as **discrete, independent
modules on a shared menu/UI + HAL shell** — each tool owns its radio setup,
runs, and tears down; adding or removing a tool never touches another.
Onboard tools ship first; RF Pack tools land once the pack is in hand.

### Authorized-use scope (applies to every tool)

Every capability is designed for use against hardware and networks the
operator owns or is explicitly authorized to test. **In scope:** capture,
replay, decode, scan, analyze. **Not built as headline features:**
untargeted denial of service — broadband jamming, deauth / beacon floods,
identifier spam. Where a primitive has both a diagnostic and a disruptive
use, it is targeted, rate-limited, off by default, and documented as to
intent. No feature exists solely to evade detection by a party the operator
is not authorized to test.

## Hardware

### Onboard — M5Stack StickS3 (ESP32-S3-PICO-1-N8R8)

| Resource | Detail |
|---|---|
| MCU | ESP32-S3, dual Xtensa LX7 @ 240 MHz, 8 MB flash, **8 MB Octal PSRAM** |
| Wi-Fi | 2.4 GHz b/g/n — STA + SoftAP, promiscuous/monitor mode, raw 802.11 TX |
| Bluetooth | BLE 5 (LE) — scan, advertise, GATT, HID peripheral |
| USB | native USB-C (USB-Serial/JTAG + USB-OTG) — can enumerate as a **HID device** to a host |
| IR | transmitter **GPIO 46**, receiver **GPIO 42** |
| IMU | BMI270 6-axis, internal I2C `0x68`, INT on GPIO 4 |
| Audio | ES8311 codec `0x18` + MEMS mic + AW8737 amp / 8 Ω speaker; I2S MCLK 18 / BCLK 17 / WS 15 / DOUT 14 / DIN 16; PA enable = M5PM1 GPIO3 |
| Display | ST7789P3 135×240, SPI3 — MOSI 39 / SCLK 40 / DC 45 / CS 41 / RST 21 / BL 38 (PWM), 3-wire; **LCD power gated by the M5PM1 PMIC** (I2C `0x6E`, drive PM1 GPIO2 high) |
| Buttons | BtnA GPIO 11, BtnB GPIO 12 (active-low) |
| Expansion | Grove I2C (SDA 9 / SCL 10); HAT2 16-pin header — free GPIO **1, 2, 3, 5, 6, 7, 8, 43, 44** (G4 = IMU INT) |
| Power | 250 mAh Li-po via M5PM1 PMIC; USB-C 5 V |
| Console | USB-Serial/JTAG @ 115200 |

### RF Pack S3 (pingequa backpack) — not yet in hand

| Item | Detail |
|---|---|
| Radios | **nRF24L01+** w/ PA+LNA (2.4 GHz); **CC1101** w/ PA+LNA (433 MHz ISM) |
| Bus | one **shared SPI** — MOSI / MISO / SCK common; separate `CSN_A` (nRF24), `CSN_B` (CC1101); plus nRF24 CE and CC1101 GDO0/GDO2 (pin count not published) |
| Isolation | SN74 logic buffer between host and radios; LC filter on the 3V3 rail |
| Antennas | 2× top SMA (2.4 GHz + 433 MHz), included |
| Mount | M2 screws to the backplate; lands on the **HAT2 header GPIO**; Grove cutout preserved. Exact pin map = whatever pr3y/Bruce's `m5stack-sticks3` board config assigns (read as reference; drivers written from datasheets). Avoid G4 (IMU INT). |
| Bus placement | its own SPI host on HAT2 pins — no contention with the display's SPI3 |

## Tool catalog

Priority: **P1** = early, onboard, high value; **P2** = onboard, later;
**RF** = needs the RF Pack.

### Wi-Fi (onboard)

| Tool | Pri | Notes |
|---|---|---|
| AP scanner | P1 | SSID / BSSID / channel / RSSI / security; sortable list |
| Packet monitor / channel activity | P1 | promiscuous mode; per-channel frame counts, live graph |
| Beacon & probe-request analysis | P2 | client probe SSIDs, vendor OUI, presence over time |
| Handshake capture (pwnagotchi-style) | P2 | passive WPA/WPA2 EAPOL capture to SD; on-screen "mood"; no dictionary/cracking on-device |
| Captive / evil portal | P2 | SoftAP + DNS + templated login page for **authorized** phishing-resistance testing; captures to SD |
| Wardriving log | P2 | AP list + IMU heading (+ external GPS via Grove later) to SD |

### Bluetooth LE (onboard)

| Tool | Pri | Notes |
|---|---|---|
| BLE scanner | P1 | name / MAC / RSSI / services; advertisement hex dump |
| Advertisement decoder | P2 | iBeacon / Eddystone / mfr-data breakdown |
| BLE-spam detector | P2 | flags floods of spoofed advertising nearby |
| BLE HID peripheral | P2 | act as a keyboard/mouse to a paired host (authorized input testing) |

### Infrared (onboard, GPIO 46/42)

| Tool | Pri | Notes |
|---|---|---|
| Learn + replay | P1 | capture a remote button, store, replay; raw + decoded |
| Protocol decoder | P2 | NEC / RC5 / Sony / Samsung etc. |
| TV-B-Gone | P2 | sweep common power codes |

### USB (onboard)

| Tool | Pri | Notes |
|---|---|---|
| HID keystroke injection | P2 | run a stored script when plugged into a host (authorized) |

### Sensors / audio (onboard)

| Tool | Pri | Notes |
|---|---|---|
| Movement / tamper alarm | P2 | BMI270 threshold → alert |
| Gesture navigation | P2 | tilt/shake as menu input |
| Mic spectrum / tone tools | P2 | FFT display; test-tone generator |

### SubGHz — CC1101, 433 MHz (RF Pack)

| Tool | Pri | Notes |
|---|---|---|
| Frequency scan + RSSI | RF | occupancy sweep across the 433 band |
| Raw capture / replay | RF | record a burst, store, replay verbatim |
| 433 sensor decode | RF | weather stations, TPMS, doorbells, PT2262-class |
| Rolling-code analysis | RF | inspect/log rolling-code frames for study; no attack tooling |

### 2.4 GHz — nRF24L01+ (RF Pack)

| Tool | Pri | Notes |
|---|---|---|
| Band scanner / spectrum | RF | per-channel energy across 2.4 GHz |
| nRF24 sniffer | RF | promiscuous-ish capture of nRF24 traffic |
| Wireless-HID research (Mousejack-style) | RF | against the operator's own dongles only |

### Platform / meta

| Item | Pri | Notes |
|---|---|---|
| Menu / UI shell + HAL seam | P1 | prerequisite — every tool plugs in here |
| Settings + SD file browser | P1 | shared storage, config, capture management |
| Script runner | P2 | chain tools (e.g. scan → filter → replay) |
| Web UI over SoftAP | P2 | browser control + capture download |
| **MCP server** | P2 | drive the device from an agent: "scan wifi", "replay signal 3" |
| Host companion sync | P2 | push captures to a workstation |

## Consequences

- **Shell first.** The menu/UI + HAL + settings/SD layer (P1 platform row)
  is built before any tool; tools are modules against that seam.
- Onboard P1 tools (Wi-Fi AP scanner, packet monitor, BLE scanner, IR
  learn/replay) can proceed now — no RF Pack, no blockers.
- All RF-tagged tools are parked until the RF Pack S3 arrives; their drivers
  (nRF24L01+, CC1101) are written from the chip datasheets, with Bruce's
  board files read only for the HAT2 pin assignment.
- The authorized-use scope above is binding on every tool's design — no
  untargeted-DoS headline features; dual-use primitives are gated.
- This ADR is a living target: new tool ideas are added here; a change that
  drops or redefines a committed capability is a superseding ADR.
