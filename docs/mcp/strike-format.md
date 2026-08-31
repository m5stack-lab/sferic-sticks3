# Strike & Flash format

**Status:** draft — living spec (ADR-0003 §7).
Shared shape for every captured event across toolsets, on device and on the
wire. Consumed by capture tools (`ir`, `subghz`, `wifi.monitor`, …) and the
companion MCP server.

## Strike

One captured event.

```json
{
  "v": 1,
  "id": "01J8...-0007",
  "station": "stick-1",
  "flash": "01J8...",
  "ts": "2026-08-30T21:14:03.512Z",
  "toolset": "ir",
  "kind": "nec",
  "label": "garage door",
  "payload": { "...": "kind-specific, see below" },
  "meta": { "...": "capture context, optional" }
}
```

| field | notes |
|---|---|
| `v` | format version (currently `1`) |
| `id` | unique; `<flash>-<seq>` or a ULID |
| `station` | capturing station id |
| `flash` | owning flash id (may be omitted for an ad-hoc single strike) |
| `ts` | ISO 8601. **On device without an RTC this is boot-relative ms** (`"t+12345"`); the server normalizes to wall-clock on ingest and keeps the original in `meta.device_t` |
| `toolset` | `ir` \| `subghz` \| `wifi` \| `ble` \| … |
| `kind` | sub-type within the toolset (see per-toolset docs) |
| `label` | optional human note |
| `payload` | the captured data — decoded and/or raw (below) |
| `meta` | optional: `rssi`, `frequency_hz`, `modulation`, `carrier_hz`, `gpio`, `duration_us`, `repeat_count`, `device_t` |

### Payload — two canonical forms

A strike SHOULD carry a **raw** payload whenever possible (so replay always
works) and MAY additionally carry a **decoded** payload.

**decoded** — a structured, protocol-specific object:

```json
{ "decoded": { "protocol": "NEC", "address": "0x04", "command": "0x08",
               "bits": 32 } }
```

**raw** — replayable primitive samples:

```json
{ "raw": { "encoding": "us-timings", "carrier_hz": 38000,
           "data": [9000, 4500, 560, 560, 560, 1690] } }
```

`encoding` values: `us-timings` (µs mark/space durations, IR & OOK SubGHz),
`bytes-b64` (base64 payload, digital protocols), `iq-b64` (reserved).

## Flash

A capture session grouping strikes. Usually single-toolset.

```json
{
  "v": 1,
  "id": "01J8...",
  "station": "stick-1",
  "toolset": "ir",
  "label": "living room remotes",
  "opened_at": "2026-08-30T21:13:00Z",
  "closed_at": "2026-08-30T21:20:00Z",
  "strike_count": 6
}
```

Strikes are stored separately from the flash record, not inline.

## Storage

- **On device, with SD:** `/sferic/flashes/<flash_id>.flash.json` +
  `/sferic/flashes/<flash_id>.ndjson` (one strike per line, appended as
  captured).
- **On device, no SD:** keep the last N strikes (N small, e.g. 16) in PSRAM,
  ephemeral. The active capture tool holds them; lost on exit / reboot.
- **On the wire (station → server):** NDJSON. Flash `open` / `close` are
  control lines; each strike is one line. The server assigns durable ids and
  timestamps and persists.

## Versioning

Every record carries `v`. A reader that sees a higher `v` than it knows
SHOULD keep `raw` payloads (still replayable) and ignore unknown fields.
