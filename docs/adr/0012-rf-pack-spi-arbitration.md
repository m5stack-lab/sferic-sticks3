# ADR-0012 — RF Pack shared-SPI arbitration

**Status:** Accepted (design). **Implementation scaffolded**, drivers parked
until the RF Pack S3 is in hand.
**Date:** 2026-09-02
**Deciders:** ry-ops (operator)
**Relates to:** ADR-0001 (RF Pack is a first-class add-on; authorized-use
scope), ADR-0004 (tool registry — RF tools plug in like any other),
ADR-0011 (ui_kit — RF screens build on it). Hardware facts:
`docs/hardware/rf-pack-s3.md`.

## Context

The pingequa **RF Pack S3** puts a **CC1101** (Sub-GHz) and an
**nRF24L01+** (2.4 GHz) on **one SPI bus** — SCK 5, MOSI 6, MISO 4 on the
StickS3 bottom/HAT2 header — with separate chip-selects (CC1101 CSN 2,
nRF24 CSN 8), a CC1101 IRQ on GDO0 3, and an nRF24 CE strobe on GPIO 1. A
SN74-series buffer sits between the ESP32 and both radios and enforces
hardware mutual-exclusion: only the radio whose CSN is asserted is connected
to MISO.

Both radio drivers will be written against whatever bus abstraction we pick
now, so the seam has to be decided before either driver is written. The
display is on a **separate** SPI host (SPI3, GPIO 39/40) and is not part of
this decision.

Constraints:

- Arduino framework + `SPIClass` is available; M5Unified owns the display
  bus only.
- The two radios are **never** used at the same time (single-threaded
  `loop()`, one active tool at a time — same rule the `stream` module
  already enforces for radios).
- CC1101 bring-up is order-sensitive on pingequa's unbuffered modules
  (issue: "CC1101 NOT FOUND" until the bus is initialised by touching the
  nRF24 first). The S3's buffer should fix this; we still guard for it.
- GPIO 4 (MISO) is shared with the BMI270 INT net — see the caveat in
  `docs/hardware/rf-pack-s3.md`. Out of scope here; noted as a bring-up gate.

## Decision

**One dedicated `SPIClass` instance owned by a single module
(`src/rf_pack.*`), shared by both radio drivers, with per-transaction chip
selection and a software "one radio active at a time" lock backed by the
board's hardware mutual-exclusion.**

Concretely:

1. **`rfpack::bus()` → `SPIClass&`.** A file-scope `SPIClass g_bus(FSPI)` in
   `rf_pack.cpp`. `rfpack::begin()` calls `g_bus.begin(SCK=5, MISO=4,
   MOSI=6)` **once**, before either radio is probed, and drives both CSN
   lines high and CE/GDO0 to their idle states. The CC1101 and nRF24
   drivers never call `SPIClass::begin()` themselves and never construct
   their own bus.

2. **Per-transaction CS + `SPISettings`.** Every register access is
   `bus.beginTransaction(SPISettings(freq, MSBFIRST, SPI_MODE0))` →
   `digitalWrite(csn, LOW)` → transfer → `digitalWrite(csn, HIGH)` →
   `bus.endTransaction()`. Both chips are SPI mode 0. Start at **4 MHz**
   for both during bring-up (`docs/hardware/rf-pack-s3.md`); raise per
   datasheet once verified. CS is plain `digitalWrite`, not the hardware
   CS of the SPI peripheral — the SN74 mux keys off the CSN level.

3. **Radio-active lock.** `rfpack::acquire(Radio) / release()` — a tiny
   guard that refuses a second radio while one is held and returns a typed
   `radio_busy`. This is redundant with the hardware mutual-exclusion but
   makes the failure explicit and keeps behaviour identical if a future
   pack drops the buffer. The existing `stream` single-session rule and the
   "one Screen at a time" UI already prevent concurrent use; this is the
   backstop.

4. **Bring-up order + retry.** `rfpack::begin()`:
   `g_bus.begin()` → probe nRF24 (`W_REGISTER` scratch round-trip) → probe
   CC1101 (`VERSION` reg == `0x14`/`0x17`). If the CC1101 probe fails, do
   one dummy nRF24 read and retry the CC1101 probe once. Record
   `{cc1101:bool, nrf24:bool}` in `rfpack::presence()`.

5. **Absent-hardware behaviour.** If a radio doesn't probe, its tools still
   register (so the menu and MCP `tools/list` are stable) but the Screen
   renders "RF Pack not detected" and the headless `run()` returns
   `{"error":"radio_absent"}`. No hang, no boot-loop.

6. **Build gate.** The whole RF toolset compiles only under
   `-DSFERIC_RF_PACK` (env `sferic-rfpack` in `platformio.ini`), mirroring
   the `sferic-usbhid` split from ADR-0005. Stock `sferic` / `sferic-usbhid`
   images stay lean and carry no "not detected" menu entries. A
   `sferic-rfpack` station declares the `subghz` + `nrf24` toolsets to the
   collector.

## Alternatives considered

- **Two `SPIClass` instances on the same pins (one per radio).** No
  benefit — the pins and the buffer are shared, so it just doubles the
  peripheral setup and invites both drivers to fight over bus config. A
  single owned bus with per-transaction `SPISettings` is the idiom.
- **Reuse the display's SPI3 host.** Rejected in ADR-0001 already;
  re-confirmed. Different pins, and sharing the panel bus would couple
  radio timing to frame draws.
- **Bit-bang SPI.** Unnecessary — hardware SPI is free on these pins and
  the ESP32-S3 has spare hosts. Bit-bang only if a pin conflict forced it,
  which it doesn't.
- **Hardware-CS (let the SPI peripheral toggle CS).** The ESP32 SPI
  hardware-CS lines aren't on GPIO 2 / 8, and the SN74 mutual-exclusion
  wants a plain level on CSN. Manual `digitalWrite` CS is simpler and
  matches how CC1101/nRF24 Arduino drivers universally do it.
- **No software lock, trust the buffer.** Rejected as the primary
  mechanism — a typed `radio_busy` is cheap and keeps the contract stable
  across board revisions. The buffer stays as the physical backstop.

## Consequences

- `src/rf_pack.*` is the one place that knows the RF Pack pin numbers and
  owns the bus; `src/subghz.*` and `src/nrf24.*` are pure register logic on
  top of `rfpack::bus()` + their CSN.
- Adding `subghz.*` / `nrf24.*` tools is the same three-part pattern as
  every existing toolset (Screen + optional headless `run()` + optional
  `StreamTool`), so ADR-0004 and the `stream` module need no changes.
- First-hardware bring-up has a written checklist: the GPIO 4 / BMI270-INT
  confirmation, the MISO scope check, `VERSION`/`PARTNUM` expected values,
  and the CC1101 init-order retry — all in `docs/hardware/rf-pack-s3.md`.
- The register-level helpers and scan sweeps committed with this ADR are
  written from the datasheets and marked `UNVERIFIED`. They compile and
  the screens are reachable; they are not claimed to work until confirmed
  on the board.
- If a later RF backpack (different radios, no buffer) appears, this ADR is
  superseded, not amended — the bus-owner seam is designed to absorb that.
