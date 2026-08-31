# MCP contract: `sensors` toolset

**Status:** draft — living spec (ADR-0003 §7).
**Firmware:** `src/motion.cpp` (`MotionScreen`) — on-device only so far.
**Toolset id:** `sensors` · **Hardware:** BMI270 6-axis IMU, ES8311 mic
(planned). No radio — no `radio_busy` rule.

All tools take `station` (required); can return `capability_not_present` or
`capability_disabled`.

---

## `sensors.motion`  — implemented (firmware: Motion / Tamper)

Watch the IMU for movement above a threshold; emit a strike per event.

**Params**

| name | type | default | notes |
|---|---|---|---|
| `station` | string | — | required |
| `sensitivity` | `"low"` \| `"med"` \| `"high"` | `"med"` | accel-deviation + gyro thresholds |
| `duration_ms` | int | `0` | `0` = run until `sensors.motion_stop`; else auto-stop |

**Returns** — a **flash** of strikes, one per detected motion event:

```json
{
  "v": 1, "station": "stick-1", "toolset": "sensors", "kind": "motion",
  "payload": { "decoded": { "accel_dev_g": 0.42, "gyro_dps": 61 } },
  "meta": { "sensitivity": "med" }
}
```

---

## `sensors.mic`  — implemented (firmware: Mic Spectrum)

`{ station, duration_ms }` → `{ peak_hz, peak_mag, bins?[] }` — a 256-pt
FFT of the ES8311 mic at 16 kHz (62.5 Hz bins, 8 kHz Nyquist).

## `sensors.gesture`  — implemented (firmware: Gesture)

`{ station, duration_ms }` → live `{ orientation, accel:[x,y,z], gyro_dps,
shakes }`; orientation ∈ face up/down, tilt left/right/up/down. Strike per
shake / orientation change lands with the headless path.

## Planned (ADR-0001)

- **`sensors.imu_read`** — `{ station }` → one instantaneous
  `{ accel:[x,y,z], gyro:[x,y,z], temp }`.

---

**Registry (ADR-0004):** the server's `tools/list` returns this toolset's
tools only when they are enabled on the target station; a disabled tool
returns `capability_disabled`. `sferic.tool_enable` / `sferic.tool_disable`
flip the state.
