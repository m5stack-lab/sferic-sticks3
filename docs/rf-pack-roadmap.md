# RF Pack S3 — bench pass + tool roadmap

**Status:** living. Hardware is in hand and both radios probe
(`[rfpack] cc1101=yes nrf24=yes`). The scan tools are bench-confirmed; the
capture / replay / decode / sniff paths are wired and non-crashing but the
register configs are datasheet defaults and **not yet confirmed against real
RF**. This doc is (1) the checklist to close that gap and (2) where the
tools go next, incl. what Bruce already does vs. what's white space for
sferic.

---

## 1. Bench pass — what needs a human with RF sources

Do these in order; each unblocks the ones under it. Driver knobs are in
`src/subghz.cpp` (`kOok[]` table) and `src/nrf24.cpp` (`sniffUp()`).

Serial control (`docs/mcp/serial-transport.md`), one `>`-line per command:

```
>{"id":1,"op":"run","tool":"subghz.capture","params":{"ms":8000}}
```

### 1a. CC1101 OOK receive — gates subghz.capture / decode / replay + every derived tool

**Need:** any cheap 433 MHz OOK remote — a socket-plug remote, a wireless
doorbell, a cheap keyfob (EV1527 / PT2262).

1. `subghz.capture` with `ms` ~8000, press the remote during the window.
   - **Good:** not `timeout`; `payload.raw.data` is 50–400 alternating
     values roughly 200–2000 µs, with a repeating sub-pattern;
     `meta.edge_count` matches.
   - **Bad → noise/AGC:** returns data but values are mostly <60 µs or a
     wall of `65535`. Tune, rebuild, retry:
     - `MDMCFG4` (reg `0x10`, RX filter BW): try `0xF6`, `0xC7`, `0x87`.
     - `AGCCTRL2` (`0x1B`): try `0x03`, `0x04`, `0xC7`.
     - `AGCCTRL0` (`0x1D`): try `0x92`, `0xB2`.
   - **Bad → always `timeout`:** RX chain not demodulating. Scope **GPIO 3
     (GDO0)** while transmitting — it must toggle. If it's dead: check
     `IOCFG0`=`0x0D` (async serial out), `PKTCTRL0`=`0x30`, and that the
     base-freq write matches the remote's actual band (315 vs 433 vs 868 —
     read the FCC ID / the sticker).
2. Once `payload.raw.data` looks real, check `payload.decoded` appears for
   an EV1527/PT2262 fob. If raw is good but decode is absent, the
   `decodePt2262()` thresholds in `subghz.cpp` need widening for this
   remote's timing ratio.
3. `subghz.decode` stream (`stream_start` / `stream_stop`) — hold the fob
   down, expect `code` strikes.

### 1b. CC1101 OOK transmit — gates subghz.replay

**Need:** an RTL-SDR running `rtl_433 -A`, a second CC1101, or just the
device the remote controls.

1. Capture a burst (1a), then `emit_arm` + `emit_fire` `subghz.replay`
   (`raw` = the captured `payload.raw.data`, `repeats` 5).
2. **Good:** `rtl_433` prints the same code, or the target device reacts.
3. **Bad:** scope GPIO 3 during fire — the MCU must drive it. Check
   `PATABLE` (`{0x00,0xC0}`), `FREND0`=`0x11`, and that `STX` was strobed
   before the bit-bang. Raise `PATABLE[1]` toward `0xC0`/`0xC3` for more
   output.

### 1c. nRF24 promiscuous sniff — gates nrf24.sniff / analyzer / replay / hid_detect

**Need:** a 2.4 GHz source you own — a bare nRF24 module, a wireless
mouse/keyboard dongle (Logitech Unifying is 2.4 GHz GFSK), an RC controller.

1. `stream_start nrf24.sniff` sweeping `ch` 2…80.
   - **Good:** frames cluster on a few channels *while the device
     transmits* and go quiet when it stops; payloads have visible structure
     (repeated prefix, stable length).
   - **Bad (current state):** constant 32-byte frames at max rate on every
     channel regardless — pure noise. Tune `sniffUp()` in `src/nrf24.cpp`:
     - `RF_SETUP` (`0x06`) data rate — **match the target**: `0x00` 1 Mbps,
       `0x08` 2 Mbps, `0x26` 250 kbps.
     - pseudo-address (`RX_ADDR_P0`) — try `{0xAA,0xAA}`, `{0x00,0xAA}`,
       `{0x55,0x00}`.
     - dwell longer per channel before reading the FIFO.
2. Add a CRC-plausibility / length filter once a real device is framing.

### 1d. Open items from `docs/hardware/rf-pack-s3.md`

- **GPIO 4 / BMI270-INT:** run `subghz.scan` then `sensors.motion` headless
  back-to-back — confirm the IMU still reads. Ideally scope MISO (GPIO 4)
  during a CC1101 status read for contention from the IMU INT net.
- **Brown-out:** on battery, `status` before/after a `subghz.replay` burst —
  watch for a reset or a battery-% dip. The pack's 100 µF should hold it.

---

## 2. Tool roadmap

### Shipped

`subghz.scan` · `subghz.capture` · `subghz.replay` · `subghz.decode`
(PT2262/EV1527) · `subghz.waterfall` · `nrf24.scan` · `nrf24.sniff` ·
`nrf24.waterfall`

### Next — cheap once §1 is green (same files, verified base)

| Tool | Toolset | Notes |
|---|---|---|
| `subghz.decode` protocol packs | subghz | TPMS, weather (Acurite/LaCrosse/Oregon), doorbell/PIR. rtl_433 is the reference. Passive RX. Wants native unit tests with captured sample timings. |
| `subghz.foxhunt` | subghz | Big RSSI number + trend on one fixed freq for walking down a transmitter. |
| `subghz.testgen` | subghz | Emit a known OOK pattern to validate your own receiver. |
| `nrf24.analyzer` | nrf24 | Per-channel dwell + packet-rate histogram → fingerprint a hopping device's channel set. |
| `nrf24.replay` | nrf24 | Capture with sniff, retransmit (your own gear). Symmetric with `subghz.replay`. |
| `nrf24.hid_detect` | nrf24 | Passively flag vulnerable 2.4 GHz HID dongles — the *detection* side of MouseJack, no injection. |
| `nrf24.txtest` | nrf24 | `CONT_WAVE` carrier for antenna/range checks. |

### The white space — what Bruce does NOT do (sferic's angle)

Bruce is a single handheld: CC1101 capture/replay/scan/**jam**/brute-force,
nRF24 scanner/**jammer**/MouseJack-inject. sferic deliberately skips the
jam/brute/inject headline features (ADR-0001) — so competing there is
pointless. Where sferic is actually different: **it's a fleet with a
collector, and it has an IMU, and it leans defensive.** Tools that fall out
of that and have no Bruce equivalent:

| Tool | Combines | Why it's novel |
|---|---|---|
| **RF direction finder** | CC1101/nRF24 RSSI + BMI270 gyro heading + display | Polar plot that points at a transmitter. `wardrive` already logs gyro bearing. |
| **RF wardrive** | `subghz.decode` / `nrf24.scan` + gyro track + collector | Walk a site, every 433 strike / 2.4 GHz hit tagged with relative heading, streamed to the Pi. |
| **Fleet RF correlation** | strikes → `sferic-collector` | Multiple stations hear the same 433 emitter → collector de-dups / rough-locates by RSSI + arrival time. This is the lightning-detection architecture (ADR-0003) applied to any emitter — the marquee feature. |
| **RF tripwire / trigger** | `subghz.decode` + tool registry | A specific 433 code (a keyfob press) fires a station action — start a Wi-Fi capture, screenshot, buzz. For unattended runs. |
| **Replay-attack detector** | `subghz.decode` | Flag the same OOK code seen twice with identical timing. Defensive, mirrors the deauth detector. |
| **Jammer detector** | CC1101 / nRF24 wideband RSSI | Flag a sustained broadband carrier on 433 or 2.4. Detection only. |
| **Full-spectrum glance** | 433 (CC1101) + 2.4 (nRF24) + Wi-Fi + BLE on one screen | One occupancy view across all four radios. Needs a cross-toolset `rf` id + Wi-Fi/BLE sweep orchestration — small ADR first. |

### Out of scope (ADR-0001) — inspect/log only, never as a build target

Sub-GHz jamming, garage/rolling-code brute-force, MouseJack keystroke
injection, any untargeted-DoS feature.
