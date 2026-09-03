# Hardware: RF Pack S3 (pingequa) on the M5Stack StickS3

**Status:** reference — not yet bench-verified (board not in hand).
**Source of the pin map:** pr3y/Bruce `boards/m5stack-sticks3/m5stack-sticks3.ini`
+ `boards/m5stack-sticks3/connections.md` — the reference ADR-0001 names for
the HAT2 assignment. Drivers are written from the CC1101 / nRF24L01+
datasheets; Bruce is read only for which GPIO each line lands on.
**Vendor docs:** github.com/pingequalab/rf-pack-s3 (setup/firmware only — no
pin table published), pingequa.com product + wiki.

The RF Pack S3 is a plug-on backpack for the StickS3: a **CC1101** (Sub-GHz,
sold as 433 MHz) and an **nRF24L01+** (2.4 GHz) on one shared SPI bus, behind
a **SN74-series logic buffer** that also does hardware mutual-exclusion
between the two radios. Power is a local **ME6211 500 mA LDO + 100 µF
tantalum** off the StickS3 3V3 rail.

---

## Pin map (Bruce `m5stack-sticks3` board config)

### Shared SPI bus — bottom / HAT2 header

| Line | GPIO | Notes |
|---|---|---|
| SCK  | **5** | not GPIO 0 — G0 is tied to the PMIC and cannot drive SCK on this board (Bruce `connections.md`) |
| MOSI | **6** | |
| MISO | **4** | **see the GPIO 4 caveat below** |

The same three pins carry the (optional) HAT2 SD card and any other
header-SPI peripheral; each device gets its own CS.

### CC1101 (Sub-GHz)

| Line | GPIO | Bruce flag |
|---|---|---|
| CSN / SS | **2** | `CC1101_SS_PIN=2` |
| GDO0 | **3** | `CC1101_GDO0_PIN=3` — packet / sync-word / RSSI-ready IRQ |
| GDO2 | — | not connected |
| SCK / MOSI / MISO | 5 / 6 / 4 | shared |

### nRF24L01+ (2.4 GHz)

| Line | GPIO | Bruce flag |
|---|---|---|
| CSN / SS | **8** | `NRF24_SS_PIN=8` |
| CE | **1** | `NRF24_CE_PIN=1` — TX/RX enable strobe |
| IRQ | — | not connected (poll `STATUS` / `RPD`) |
| SCK / MOSI / MISO | 5 / 6 / 4 | shared |

### Not used by the RF Pack (same header, other functions)

GPIO 7 = HAT2 SD CS · GPIO 43 / 44 = header UART TX / RX (`SERIAL_TX=43`,
`SERIAL_RX=44`) · Grove I2C on 9 / 10 · system I2C (PMIC, IMU, codec) on
47 / 48.

---

## GPIO 4 caveat (confirm on first bring-up)

ADR-0001's "HAT2 free GPIO" list is **1, 2, 3, 5, 6, 7, 8, 43, 44** and calls
GPIO 4 the BMI270 interrupt line. Bruce's board config nonetheless routes the
header SPI **MISO to GPIO 4** and runs SD + CC1101 + nRF24 on it. On the
StickS3, GPIO 4 is shared between the BMI270 `INT` and the bottom-header MISO
net.

sferic reads the IMU by **polling over I2C** (`M5.Imu.getAccel/getGyro` in
`motion.cpp` / `gesture.cpp`) and never enables the BMI270 interrupt, so
GPIO 4 is available as MISO. First bring-up MUST confirm:

1. M5Unified does not configure the BMI270 `INT` pin as a push-pull output
   (it should be left as a plain input / hi-Z). If it drives it, disable the
   BMI270 INT in `hal.begin()` before `rfpack::begin()`.
2. Scope MISO during a CC1101 `VERSION` read and an nRF24 `CONFIG` read —
   clean edges, no contention from the IMU line.

Every RF-Pack GPIO the pack actually uses (**1, 2, 3, 5, 6, 8**) is inside
ADR-0001's free list. GPIO 4 is the only reconciliation item.

---

## Electrical / bring-up notes

- **Power:** the pack regulates its own 3V3 (ME6211, 500 mA) from the
  StickS3 3V3 pin; the 100 µF tantalum holds the rail up during TX bursts.
  No power-sequencing work on our side beyond "3V3 is present at boot".
- **SN74 buffer + hardware mutual exclusion:** the two radios share MISO
  through the buffer, which enables only the radio whose CSN is currently
  asserted. There is **no dedicated band-select GPIO** — band selection is
  implicit in which CSN we pull low. Software must still never drive both
  CSN lines low at once (see ADR-0012).
- **SPI-init ordering quirk:** on the unbuffered pingequa 2-in-1 modules the
  CC1101 fails to come up unless the SPI bus was already initialised
  (typically by touching the nRF24 first) — Bruce issue #1909, pingequa
  "CC1101 NOT FOUND" troubleshooting. The RF Pack S3's SN74 buffer is meant
  to make order irrelevant, but pingequa still ship "CC1101 (legacy)" as the
  recommended driver path. Mitigation in `rfpack::begin()`: call
  `bus.begin(5, 4, 6)` explicitly before either radio; if the CC1101
  `VERSION`/`PARTNUM` probe fails, do one dummy nRF24 register read and
  retry once.
- **CC1101 `VERSION` register:** expect `0x14` (older die) or `0x17`.
  **`PARTNUM`** expect `0x00`.
- **nRF24 presence check:** write a non-reset value to a scratch register
  (`EN_AA` = `0x00`, or `SETUP_AW`) via `W_REGISTER`, read it back, restore.
  A floating CSN reads back `0x00`/`0xFF` and fails the round-trip.
- **Clock:** start conservative — 4 MHz SPI for both radios during bring-up,
  raise later. Display SPI3 (GPIO 39/40) is a separate host — no contention.

---

## What the drivers need to implement (parked until the pack ships)

Scaffold in `src/rf_pack.*`, `src/subghz.*`, `src/nrf24.*` today (behind
`SFERIC_RF_PACK`, env `sferic-rfpack`). Register-level helpers and the scan
sweeps are written from the datasheets and marked `UNVERIFIED`; fill in and
confirm on first hardware.

| Tool | Chip | Primitive |
|---|---|---|
| `subghz.scan` | CC1101 | tune FREQ2/1/0 across the band, `SRX`, read `RSSI` reg → occupancy sweep |
| `subghz.capture` / `subghz.replay` | CC1101 | async / raw FIFO burst record + verbatim replay |
| `subghz.decode` | CC1101 | OOK/ASK framing → PT2262-class / TPMS / weather |
| `nrf24.scan` | nRF24 | set `RF_CH`, CE pulse, read `RPD` (carrier detect) per channel → 2.4 GHz energy map |
| `nrf24.sniff` | nRF24 | promiscuous-ish capture with a shortened address + `EN_DPL` |

All RF tools follow the existing patterns: a `Screen` subclass
(`onEnter`/`tick`/`onExit`), an optional headless `run()` for MCP, and — for
the continuous ones — a `StreamTool` (`src/stream.*`). They obey the
authorized-use scope in ADR-0001 (capture / scan / decode / replay; no
untargeted-DoS headline features).
