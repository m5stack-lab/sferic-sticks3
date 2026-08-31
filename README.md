# sferic

ESP32-S3 hardware / radio multitool firmware for the **M5Stack StickS3**.
MIT licensed. In the spirit of pr3y/Bruce — reference only, not a fork.

The architecture is a lightning-detection network: distributed **stations**
(M5 devices) capture RF impulses, a central collector does time-of-arrival
work. Vocabulary follows the field — station, **strike** (one event),
**flash** (a capture session). See
[`docs/adr/0002-project-name-sferic.md`](docs/adr/0002-project-name-sferic.md).

## Build & flash

```sh
pio run -e sferic                  # build
pio run -e sferic -t upload        # flash (StickS3 on USB-C)
pio device monitor -b 115200       # serial (assert DTR)
```

Toolchain: PlatformIO + **Arduino framework** + M5Unified. (ESP-IDF is a
dead end here — M5GFX crashes in its SPI init under IDF 5.3.)

## On-device controls

Two buttons: **A** = next / (hold) prev · **B** = select / (hold) back.
Over serial you can also type `a` `A` `b` `B` to drive the menu.

## What's here

- **Menu / HAL shell** — `src/hal.*` (M5Unified wrapper, Click/Hold button
  events), `src/ui.*` (`Screen` + `ScreenStack`, `MenuScreen`, `InfoScreen`),
  `src/menu_tree.*` (the root menu from ADR-0001).
- **WiFi → AP Scan** (`src/wifi_scan.*`) — async 2.4 GHz scan; SSID / RSSI /
  channel / encryption, colour-coded by signal; A = next, A-hold / B =
  rescan.
- Everything else in the menu is a stub — see
  [`docs/adr/0001-onboard-rf-and-tool-capabilities.md`](docs/adr/0001-onboard-rf-and-tool-capabilities.md)
  for the full tool catalog and build order.

## Hardware

M5Stack StickS3 — ESP32-S3-PICO-1-N8R8, 8 MB flash / 8 MB Octal PSRAM,
ST7789P3 135×240, BMI270 IMU, ES8311 audio, IR TX/RX, M5PM1 PMIC.
Planned add-on: pingequa **RF Pack S3** (nRF24L01+ + CC1101). Pin map in
ADR-0001.
