# MCP contract: `subghz` toolset

**Status:** draft — scaffolded, hardware not in hand (ADR-0001, ADR-0012).
**Firmware:** `src/subghz.cpp` + shared bus `src/rf_pack.cpp`. Compiled only
in the `sferic-rfpack` build (`-DSFERIC_RF_PACK`). Register logic is written
from the CC1101 datasheet and **UNVERIFIED**.
**Toolset id:** `subghz` · **Radio:** CC1101 on the RF Pack S3 shared SPI bus
(SCK 5 / MOSI 6 / MISO 4, CSN 2, GDO0 3). Pin map + bring-up notes:
`docs/hardware/rf-pack-s3.md`.

Shares one SPI bus and a one-radio-at-a-time lock with `nrf24` (ADR-0012).
A call while the nRF24 is active returns `radio_busy`. A call on a station
whose CC1101 did not probe returns `radio_absent`; a station without the RF
Pack build returns `capability_not_present`. All tools take `station`
(required).

---

## `subghz.scan` — scaffolded (firmware: Band Scan)

Sweep the 433 MHz ISM band and report per-step RSSI + the peak.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `ms` | int | `4000` | time budget for the sweep |

**Returns**

```json
{
  "band": "433",
  "points": [
    { "mhz": 433.05, "rssi": -108 },
    { "mhz": 433.14, "rssi": -96 }
  ],
  "peak": { "mhz": 433.92, "rssi": -61 }
}
```

Sweep span is 433.05–434.79 MHz in 20 steps (`SubGhzScanScreen::kSteps`).
`rssi` is dBm, decoded from the CC1101 `RSSI` status register with the
datasheet 433 MHz offset. Errors: `radio_absent`, `radio_busy`.

---

## `subghz.capture` — implemented (firmware: Learn / Replay)

Record one raw OOK burst from the CC1101 (async serial demod on GDO0) and
return it as a strike.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `ms` | int | `5000` | listen budget; returns as soon as a burst frames |
| `mhz` | float | `433.92` | carrier |

**Returns** — a strike per `docs/mcp/strike-format.md`:

```json
{
  "v": 1, "toolset": "subghz", "kind": "ook", "ts": "t+41234",
  "meta": { "frequency_hz": 433920000, "modulation": "OOK",
            "duration_us": 18240, "edge_count": 132 },
  "payload": {
    "raw": { "encoding": "us-timings", "data": [420, 1280, 1290, 410, "…"] },
    "decoded": { "protocol": "PT2262", "bits": 24, "value": "0x1A2B3C",
                 "te_us": 420 }
  }
}
```

`payload.decoded` is present only when the burst frames as a PT2262 / EV1527
(rc-switch protocol 1) code. Errors: `radio_absent`, `radio_busy`, `timeout`.

## `subghz.replay` — implemented (emit; arm/fire)

Re-transmit a captured OOK burst verbatim on the CC1101 (async TX, MCU keys
GDO0). Two-step like every emitter — `emit_arm` then `emit_fire`
(`docs/mcp/headless.md`).

**Params** (`emit_arm`)

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `raw` | int[] | — | µs mark/space, `[0]` = mark. Also accepts a full strike object (`payload.raw.data`) |
| `mhz` | float | `433.92` | carrier |
| `repeats` | int | `5` | 1–20 frame repeats |

**Returns** (`emit_fire`): `{ "sent": true, "frames": 5 }`. Errors:
`radio_absent`, `radio_busy`, `bad_params`.

> **UNVERIFIED:** the CC1101 OOK config and the async-TX keying are the
> standard SmartRC / rc-switch values but are not yet bench-confirmed on the
> RF Pack S3 — `docs/hardware/rf-pack-s3.md`.

## `subghz.decode` — implemented (firmware: Protocol Decode)

`{ station, mhz? }` → continuous OOK framing → PT2262 / EV1527 frames
`{ protocol, bits, value, te_us }`, streamed as `code` strikes, not stored.
A `StreamTool` (`src/stream_hub.*`): `stream_start` / `stream_stop`.

---

## Planned (ADR-0001 — not yet built)

- **TPMS / weather-station / doorbell** decoders on top of `subghz.decode`
  (only PT2262/EV1527 framing today).

---

**Authorized-use scope (ADR-0001):** capture / scan / decode / replay only,
against hardware the operator owns or is authorized to test. No broadband
jamming or untargeted-DoS headline feature. Rolling-code tooling is
inspect/log only.

**Registry (ADR-0004):** `tools/list` returns `subghz.*` only when the
station declares the toolset (RF Pack build + CC1101 probed) and the tool is
enabled. `sferic.tool_enable` / `sferic.tool_disable` flip the state.
