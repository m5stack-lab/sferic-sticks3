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

## Planned (ADR-0001 — firmware stubs to fill on first hardware)

### `subghz.capture`
`{ station, mhz, modulation?, ms }` → record a raw OOK/ASK burst from the
CC1101 async FIFO, return it as a strike per `docs/mcp/strike-format.md`
(`payload.raw.data` = us-timings). No decode required.

### `subghz.replay`
`{ station, strike, repeats? }` → re-transmit a captured `subghz` strike
verbatim on the CC1101. On-device this is two-step (arm, then confirm) like
every emitter — see `src/emit.*` / `docs/mcp/headless.md`.

### `subghz.decode`
`{ station, ms }` → continuous framing of OOK/ASK traffic → PT2262-class /
TPMS / weather-station / doorbell frames `{ protocol, bits, value }`, not
stored. A `StreamTool` (`src/stream.*`).

---

**Authorized-use scope (ADR-0001):** capture / scan / decode / replay only,
against hardware the operator owns or is authorized to test. No broadband
jamming or untargeted-DoS headline feature. Rolling-code tooling is
inspect/log only.

**Registry (ADR-0004):** `tools/list` returns `subghz.*` only when the
station declares the toolset (RF Pack build + CC1101 probed) and the tool is
enabled. `sferic.tool_enable` / `sferic.tool_disable` flip the state.
