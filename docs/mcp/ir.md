# MCP contract: `ir` toolset

**Status:** draft — living spec (ADR-0003 §7).
**Firmware:** `src/ir_tool.cpp` (`IrScreen`) — on-device Learn / Replay;
no headless path yet.
**Toolset id:** `ir` · **Radios:** `ir-tx` (GPIO 46), `ir-rx` (GPIO 42)
· library: IRremoteESP8266

Independent of the 2.4 GHz radio — no `radio_busy` coexistence rule. All
tools take `station` (required) and can return `capability_not_present`.

---

## `ir.capture`  — implemented (firmware: Learn / Replay, arm + receive)

Receive one IR signal and return it as a strike.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `timeout_ms` | int | `20000` | give up if nothing is received |
| `label` | string | — | optional, stored on the strike |

**Returns** — a strike per `docs/mcp/strike-format.md`:

```json
{
  "v": 1, "station": "stick-1", "toolset": "ir",
  "kind": "NEC",
  "payload": {
    "decoded": { "protocol": "NEC", "value": "0x20DF10EF", "bits": 32 },
    "raw": { "encoding": "us-timings", "carrier_hz": 38000,
             "data": [9000, 4500, 560, 560, 560, 1690] }
  },
  "meta": { "gpio": 42 }
}
```

`kind` is the decoded protocol name, or `"raw"` when undecodable (only
`payload.raw` is present then). A `raw` payload is always included when
available so replay works regardless of protocol support.

On timeout: `{ "captured": false }`.

---

## `ir.replay`  — implemented (firmware: replay held strike)

Re-transmit a strike on the IR LED.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `strike` | string \| object | — | a strike id, or an inline strike object |
| `repeats` | int | `1` | send count |

**Returns** `{ "sent": true, "method": "decoded" }` — `method` is `decoded`
when resent via the protocol, `raw` when replayed from the timing buffer.

---

## Planned (firmware stubs — ADR-0001)

## `ir.decode`  — implemented (firmware: Protocol Decode)

`{ station, duration_ms }` → a stream / list of decoded hits
`{ protocol, value, bits }` (or `{ raw: <samples> }` when undecodable),
not stored. A continuous peek.

## Planned (ADR-0001)
### `ir.tvbgone`  — implemented

`{ station, repeats? }` → sweeps a built-in set of common TV power codes
(Samsung/LG/Sony/Toshiba/Vizio/TCL/Philips/Panasonic/...); returns
`{ codes_sent: 14 }`. Best-effort by design.

---

**Registry (ADR-0004):** the server's `tools/list` returns this toolset's tools only when they are enabled on the target station. Calling a disabled tool returns `capability_disabled` (vs `capability_not_present` = the station's hardware can't do it at all). `sferic.tool_enable` / `sferic.tool_disable` flip the state.
