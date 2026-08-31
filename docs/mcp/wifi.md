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

## Planned (firmware stubs — see ADR-0001)

Sketches only; params/returns firm up when each is built.

### `wifi.monitor` — promiscuous capture → strikes

`{ station, channel | "hop", duration_ms, filter? }` → a **flash** of
strikes, one per captured frame (or per summarized burst): `{ ts, channel,
type, subtype, addr1, addr2, addr3, rssi, len }`.

### `wifi.probes`

`{ station, duration_ms }` → `{ clients: [{ mac, oui, probed_ssids: [...],
last_seen, count }] }`.

### `wifi.handshake` — EAPOL capture → flash

`{ station, target_bssid?, channel?, timeout_ms }` → a **flash** containing
the captured EAPOL frames + beacon; `{ flash_id, complete: bool }`. Passive
capture only; no on-device cracking.

### `wifi.portal`

`{ station, ssid, template, duration_ms }` → `{ flash_id, submissions: N }`.
For authorized phishing-resistance testing only (ADR-0001 scope).

---

**Registry (ADR-0004):** the server's `tools/list` returns this toolset's tools only when they are enabled on the target station. Calling a disabled tool returns `capability_disabled` (vs `capability_not_present` = the station's hardware can't do it at all). `sferic.tool_enable` / `sferic.tool_disable` flip the state.
