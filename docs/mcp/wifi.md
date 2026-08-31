# MCP contract: `wifi` toolset

**Status:** draft — living spec (ADR-0003 §7), not an ADR.
**Firmware:** `src/wifi_scan.cpp` (`WifiScanScreen`) — on-device only so far;
no headless path yet.
**Toolset id:** `wifi` · **Radio:** onboard 2.4 GHz (`wifi-2g4`)

All tools take `station` (id or friendly name; required). All can return
`capability_not_present` (station has no `wifi` toolset) or `radio_busy`
(a `ble.*` op or another `wifi.*` op is active on that station — the 2.4 GHz
radio is shared).

---

## `wifi.scan`  — implemented (firmware: AP Scan)

Scan for nearby access points.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `duration_ms` | int | `4000` | tool returns when the scan completes or this elapses |
| `hidden` | bool | `true` | include APs not broadcasting an SSID |
| `active` | bool | `true` | `false` = passive (listen only, slower, stealthier) |

**Returns**

```json
{
  "station": "stick-1",
  "scanned_at": "2026-08-30T21:14:03Z",
  "count": 19,
  "aps": [
    { "ssid": "example", "bssid": "aa:bb:cc:dd:ee:ff", "rssi": -52,
      "channel": 6, "band": "2.4", "encryption": "WPA2", "hidden": false }
  ]
}
```

`encryption` enum: `OPEN` `WEP` `WPA` `WPA2` `WPA12` `WPA2E` `WPA3` `WPA23` `?`.
`aps` sorted by `rssi` descending.

---

## `wifi.monitor`  — implemented (firmware: Packet Monitor)

Promiscuous 802.11 capture. Firmware today reports **counts** (total /
mgmt / data / ctrl / rate / last RSSI) on a fixed or hopping channel; the
per-frame → strikes path lands with the headless work.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `channel` | int \| `"hop"` | `1` | `"hop"` cycles 1–13 (~4/s) |
| `duration_ms` | int | `10000` | window to summarize over |

**Returns** — a summary now; a **flash** of per-frame strikes once headless:

```json
{ "station": "stick-1", "channel": 1, "elapsed_ms": 10000,
  "total": 512, "mgmt": 498, "data": 12, "ctrl": 2,
  "rate_per_s": 51, "last_rssi": -80 }
```

Per-frame strike shape (planned): `{ ts, channel, type, subtype, addr1,
addr2, addr3, rssi, len }`.

## `wifi.probe`  — implemented (firmware: Probe / Beacon)

`{ station, duration_ms }`, auto-hops channels → `{ clients: [{ mac,
probed_ssid, rssi, count, last_seen }] }` (one entry per unique mac+ssid;
empty ssid = wildcard probe).

## `wifi.portal`  — implemented (firmware: Evil Portal)

`{ station, ssid, duration_ms }` → open SoftAP + catch-all DNS + a generic
captive login page. Submissions land in `{ captures: [{ field1, field2,
ms }] }` (kept on-device, RAM only, never forwarded). Generic page, not
brand-impersonating. Authorized phishing-resistance testing only
(ADR-0001 scope).

## Planned (firmware stubs — see ADR-0001)

### `wifi.handshake` — EAPOL capture → flash

`{ station, target_bssid?, channel?, timeout_ms }` → a **flash** containing
the captured EAPOL frames + beacon; `{ flash_id, complete: bool }`. Passive
capture only; no on-device cracking.

---

**Registry (ADR-0004):** the server's `tools/list` returns this toolset's tools only when they are enabled on the target station. Calling a disabled tool returns `capability_disabled` (vs `capability_not_present` = the station's hardware can't do it at all). `sferic.tool_enable` / `sferic.tool_disable` flip the state.
