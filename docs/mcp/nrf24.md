# MCP contract: `nrf24` toolset

**Status:** draft — scaffolded, hardware not in hand (ADR-0001, ADR-0012).
**Firmware:** `src/nrf24.cpp` + shared bus `src/rf_pack.cpp`. Compiled only
in the `sferic-rfpack` build (`-DSFERIC_RF_PACK`). Register logic is written
from the nRF24L01+ datasheet and **UNVERIFIED**.
**Toolset id:** `nrf24` · label "2.4GHz" · **Radio:** nRF24L01+ on the RF
Pack S3 shared SPI bus (SCK 5 / MOSI 6 / MISO 4, CSN 8, CE 1). Pin map +
bring-up notes: `docs/hardware/rf-pack-s3.md`.

Shares one SPI bus and a one-radio-at-a-time lock with `subghz` (ADR-0012).
A call while the CC1101 is active returns `radio_busy`. A call on a station
whose nRF24 did not probe returns `radio_absent`; a station without the RF
Pack build returns `capability_not_present`. All tools take `station`
(required).

> Note: the on-board ESP32 Wi-Fi/BLE radio also lives at 2.4 GHz but is a
> **separate** radio — `nrf24.*` never conflicts with `wifi.*` / `ble.*`,
> only with `subghz.*` (shared SPI bus).

---

## `nrf24.scan` — scaffolded (firmware: Band Scan)

Poor-man's 2.4 GHz spectrum: step every channel, pulse CE, sample the
Received Power Detector (`RPD`) bit, accumulate carrier-detect hits over
repeated sweeps.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `ms` | int | `4000` | time budget; more time = more sweeps = smoother |

**Returns**

```json
{
  "sweeps": 61,
  "channels": [0, 0, 3, 11, 40, 7, 0, "… 126 entries, index = channel"],
  "peak_ch": 4,
  "peak_mhz": 2404,
  "peak_hits": 40
}
```

`channels[c]` is the carrier-detect count for channel `c` (2400 + c MHz)
across `sweeps`. RPD only reports energy above roughly −64 dBm, so this is a
coarse occupancy map, not a calibrated spectrum. Errors: `radio_absent`,
`radio_busy`.

---

## `nrf24.sniff` — implemented (firmware: Sniff)

Promiscuous-ish capture: a 2-byte pseudo-address (`0x55 0x55`, Goodspeed's
preamble trick), CRC + auto-ack off, fixed 32-byte payload. Drains the RX
FIFO and streams one strike per frame. A `StreamTool` (`src/stream_hub.*`):
`stream_start` / `stream_stop`.

**Params** (`stream_start`)

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `ch` | int | `2` | 2.4 GHz channel (2400 + `ch` MHz); change and re-scan on device with `A` |

**Strike** (`kind: "frame"`):

```json
{ "toolset": "nrf24", "kind": "frame",
  "payload": { "decoded": {
    "bytes": 32,
    "raw": { "encoding": "bytes-b64", "data": "VVVV…" } } } }
```

> The stream envelope (`src/stream_hub.cpp`) nests every tool's emit under
> `payload.decoded`, so the raw bytes land at `payload.decoded.raw` rather
> than `payload.raw`. Cosmetic; revisit if the envelope grows a raw slot.

**Noise gate.** With CRC off there's nothing to validate a frame, so the
FIFO fills with ~240/s of garbage. Two cheap filters, both must pass:
*entropy* (≥10 distinct byte values — drops the frames the addressless
receiver latches on Wi-Fi/BLE preamble energy, which are nearly all
`0x55`/`0xAA`) and *corroboration* (the 5-byte prefix must have already been
seen — random noise never recurs; a retransmitted real packet does; the
first copy is dropped). Measured: ~950/4s raw → 0/4s on quiet channels,
~50/4s worst case in the Wi-Fi band. Still **UNVERIFIED against a real
target** — tune against your own dongle (§1c of `docs/rf-pack-roadmap.md`).

> **UNVERIFIED:** the promiscuous config (illegal 2-byte `SETUP_AW`, no CRC)
> is the well-known Goodspeed / Mousejack approach but is not yet
> bench-confirmed on the RF Pack S3 — expect a high false-frame rate and
> tune `RF_SETUP` / the pseudo-address on first hardware
> (`docs/hardware/rf-pack-s3.md`). Against the operator's own
> devices/dongles only.

---

## `nrf24.waterfall` — implemented (firmware: Waterfall)

Display-only. Time-scrolling 2.4 GHz carrier-detect spectrogram — every
channel RPD-sampled a few times per row, rows stacked so Wi-Fi / BLE /
hopping traffic shows as vertical bands. No headless path. `A` clears,
`B`-hold exits.

---

## Planned (ADR-0001 — not yet built)

### `nrf24.hid_research` (Mousejack-style)
Parked. Research-only, against the operator's own wireless HID dongles.
Two-step arm/confirm like every emitter. Not built as a headline feature —
see the authorized-use scope.

---

**Authorized-use scope (ADR-0001):** scan / sniff / analyze, against
hardware the operator owns or is authorized to test. No untargeted-DoS
headline feature.

**Registry (ADR-0004):** `tools/list` returns `nrf24.*` only when the
station declares the toolset (RF Pack build + nRF24 probed) and the tool is
enabled. `sferic.tool_enable` / `sferic.tool_disable` flip the state.
