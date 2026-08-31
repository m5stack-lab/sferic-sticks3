# MCP contract: `ble` toolset

**Status:** draft — living spec (ADR-0003 §7), not an ADR.
**Firmware:** `src/ble_scan.cpp` (`BleScanScreen`) — on-device only so far;
no headless path yet.
**Toolset id:** `ble` · **Stack:** NimBLE-Arduino · **Radio:** onboard BLE (`ble`)

All tools take `station` (required). All can return `capability_not_present`
or `radio_busy` (a `wifi.*` op is active — 2.4 GHz radio is shared).

---

## `ble.scan`  — implemented (firmware: BLE Scan)

Snapshot of advertising BLE devices.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `duration_ms` | int | `5000` | firmware scans continuously; the tool returns a snapshot after this |
| `active` | bool | `true` | `false` = passive (no scan requests; misses scan-response data) |

**Returns**

```json
{
  "station": "stick-1",
  "scanned_at": "2026-08-30T21:14:03Z",
  "count": 30,
  "devices": [
    { "address": "aa:bb:cc:dd:ee:ff", "address_type": "random",
      "name": "Widget", "rssi": -61,
      "service_uuids": ["0000180f-0000-1000-8000-00805f9b34fb"],
      "appearance": 833, "connectable": true }
  ]
}
```

`name` omitted when the device advertises none. `devices` sorted by `rssi`
descending.

---

## Planned (firmware stubs — see ADR-0001)

Sketches only.

### `ble.decode`  — implemented on-device as the scan detail view

On the device, B on a scanned device opens a decode panel: address + type,
name, RSSI, connectable, adv flags, appearance, service UUIDs, manufacturer
data (company id + hex), and iBeacon parse (UUID / major / minor / tx).
MCP form: `{ station, address }` → the same fields as a struct. Eddystone
decode still TODO.

### `ble.spam_watch`

`{ station, duration_ms }` → a **flash** of strikes, one per detected
advertising flood (spoofed-device spam): `{ ts, pattern, rate, sample_addrs }`.

### `ble.hid`

Act as a BLE HID peripheral (keyboard/mouse) toward a paired host — likely
its own interaction model rather than a single call. Authorized input
testing only.

---

**Registry (ADR-0004):** the server's `tools/list` returns this toolset's tools only when they are enabled on the target station. Calling a disabled tool returns `capability_disabled` (vs `capability_not_present` = the station's hardware can't do it at all). `sferic.tool_enable` / `sferic.tool_disable` flip the state.
