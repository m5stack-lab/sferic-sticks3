# MCP contract: `ble` toolset

**Status:** draft — living spec (ADR-0003 §7), not an ADR.
**Firmware:** `src/ble_scan.cpp`. `ble.scan` has a headless one-shot `run()`
(`docs/mcp/headless.md`); `ble.spam` / `ble.gatt` / `ble.hid` are
`Screen`-only.
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
    { "address": "aa:bb:cc:dd:ee:ff", "address_type": "public",
      "name": "Widget", "rssi": -61,
      "service_uuids": ["0000180f-0000-1000-8000-00805f9b34fb"],
      "appearance": 833, "connectable": true,
      "security": { "vendor": "Espressif Inc.", "cve_vendor_known": true,
        "note": "…" } }
  ]
}
```

`name` omitted when the device advertises none. `devices` sorted by `rssi`
descending.

`security` is added by the **collector** on ingest (ADR-0010,
`sferic-collector`), same as `wifi.scan`'s `aps[]`. Only meaningful for
`address_type: "public"` — a `"random"` address (the common case for
privacy-conscious phones/wearables) carries no IEEE-assigned vendor, so
those entries get no `security` block at all rather than a misleading
`{"vendor": null}`.

---

## Planned (firmware stubs — see ADR-0001)

Sketches only.

### `ble.decode`  — implemented on-device as the scan detail view

On the device, B on a scanned device opens a decode panel: address + type,
name, RSSI, connectable, adv flags, appearance, service UUIDs, manufacturer
data (company id + hex), and iBeacon parse (UUID / major / minor / tx).
MCP form: `{ station, address }` → the same fields as a struct. Eddystone
decode still TODO.

### `ble.spam`  — implemented (firmware: Spam Detect)

`{ station, duration_ms }` → per-second `{ adv_rate, new_addr_rate,
peak_new_addr, verdict }` where verdict ∈ QUIET / BUSY / SPAM (SPAM =
adv_rate > 80/s and new_addr_rate > 40/s — an address-rotating flood).
Strike-per-flood output lands with the headless path.

## `ble.gatt`  — implemented (firmware: connect from the decode view)

On the device: BLE Scan -> a connectable device -> decode -> B connects and
enumerates. `{ station, address }` -> `{ services: [{ uuid, characteristics:
[{ uuid, props, value? }] }] }` where props is a subset of R/W/N/I;
readable characteristics are read once. Disconnects after.

## Planned (ADR-0001)

## `ble.hid`  — implemented (firmware: HID Keyboard)

Advertises as a BLE keyboard (`sferic-kb`, appearance HID keyboard). Once a
host pairs, `{ station, text }` types `text` as key events (boot-keyboard
8-byte reports, ascii subset: letters, digits, space, newline, `.-_`).
Authorized input testing only (ADR-0001).

---

**Registry (ADR-0004):** the server's `tools/list` returns this toolset's tools only when they are enabled on the target station. Calling a disabled tool returns `capability_disabled` (vs `capability_not_present` = the station's hardware can't do it at all). `sferic.tool_enable` / `sferic.tool_disable` flip the state.
