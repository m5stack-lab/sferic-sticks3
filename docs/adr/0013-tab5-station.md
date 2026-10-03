# ADR-0013 — M5Stack Tab5 as a fleet station

**Status:** Accepted (plan). Implementation not started — gated on a
bring-up spike (see §2).
**Date:** 2026-09-03
**Deciders:** ry-ops (operator)
**Relates to:** ADR-0007 (fleet composition — Tab5 = "rich touch field
station"), ADR-0009 (station↔collector transport — reused verbatim),
ADR-0011 (UI kit & theme — grammar reused, metrics parameterized),
ADR-0001 (authorized-use scope — binds the new toolsets), ADR-0004
(registry / `<toolset>.<verb>` model — board-agnostic).

## Context

The StickS3 firmware (`m5stack-lab/sferic-sticks3`) is proven: onboard tool
catalog complete, serial + WebSocket station transports, driven end-to-end
from the collector over a real wireless link (2026-09-03). The
board-agnostic seams held — `registry`, `dispatch` / `link`, the strike
format, `sferic-fake-station`.

The **M5Stack Tab5** is the next station. It is not "a bigger StickS3": a
different SoC family, a real touch display, a detachable hardware keyboard,
a camera, and radio only via a co-processor. This ADR fixes how the Tab5
firmware is structured so the shared work carries and the new work is
scoped.

### Hardware (Tab5)

| Resource | Detail |
|---|---|
| MCU | **ESP32-P4** — dual-core RISC-V @ 400 MHz, **no built-in wireless** |
| Radio | **ESP32-C6 co-processor** over SDIO/`esp_wifi_remote` — Wi-Fi 6 (2.4 GHz), BLE 5, **802.15.4** (Thread / Zigbee / Matter) |
| Display | 5" **1280×720 IPS**, MIPI-DSI, **GT911 capacitive touch** |
| Camera | **MIPI-CSI 2 MP** (SC2356) — no other sferic device has one |
| Input | **Tab5 hardware keyboard** (detachable); BMI270 IMU |
| USB | **USB 2.0 HS OTG** — host or device |
| Audio | ES8388 codec + dual mic + speaker |
| Buses | RS-485 + CAN (TJA1051) on the M-Bus; Grove; GPIO expander |
| Storage | **microSD** + large PSRAM |
| Power | battery + PMU |

### What the StickS3 taught us

1. The HAL seam works — a new board is a new `hal` implementation + a build
   env; `registry` / `dispatch` / `link` / UI kit / theme sit above it.
2. **Blocking one-shot `run()` breaks the WS link.** On the Stick a ~4 s
   blocking scan in `loop()` starved the WS pump and tripped a watchdog
   (`RTC_SW_CPU_RST`). The Stick is single-threaded cooperative, so this is
   a hard constraint there; the Tab5 has a second core.
3. Radio contention: on the Stick the one 2.4 GHz radio can't hold the
   uplink and run `wifi.*` promiscuous at once (ADR-0009 §4). The Tab5's
   radio is a separate chip.

## Decision

### 1. New repo `m5stack-lab/sferic-tab5`; copy shared files, defer a library

The Tab5 firmware is its own repo. It **copies** the board-agnostic sources
from `sferic-sticks3` (`ui_kit`, `theme`, `icons`, `registry`, `tool`,
`dispatch`, `link`, `serial_control`, `ws_station`, `stream_hub`, `emit`,
`version`, the `models`/strike shape). A shared `sferic-core` library is
**not** extracted now — premature with one prior consumer and a UI/HAL
surface that is about to change. Extraction is a later ADR once both
firmwares are real and the shared surface has stabilized. Cross-project
ADRs continue to live in `sferic-sticks3/docs/adr/`.

### 2. A bring-up spike gates the toolset

Before any tool design, a spike proves: boot on the P4 toolchain (Arduino +
M5Unified, or ESP-IDF if M5Unified's P4 support is not ready — decide in the
spike), MIPI-DSI display, GT911 touch, IMU, the keyboard, microSD, and
**the C6 link** — specifically **whether promiscuous / monitor mode works
over `esp_wifi_remote`**. That answer sets the `wifi.*` / `ble.*` scope:
full catalog if monitor mode works, scan-only if it does not. The spike is
flashed by the operator; no design commits until it reports.

### 3. HAL emits abstract input events (nav + text)

The Stick's HAL exposes `Btn::A/B` + `BtnEvent::Click/Hold`. That is
replaced across both firmwares by **abstract events** the UI and tools
consume:

- **Nav:** `Next`, `Prev`, `Select`, `Back`, `Page±`.
- **Text/key** (keyboard, on-screen keyboard): character input, and named
  keys `Enter`, `Esc`, `Tab`, arrows, plus chorded shortcuts.

Each board derives them: the Stick from BtnA/BtnB (as today), the Tab5 from
touch gestures (swipe = `Next`/`Prev`, tap-target = `Select`, edge-swipe =
`Back`) **and** the keyboard. Tools and the UI never see raw buttons,
touch coordinates, or scan codes. **The nav-event refactor lands on the
StickS3 first** — small, backward-compatible, and it means the Tab5's input
plugs into a proven seam.

### 4. UI: same grammar, board-profile metrics

`ui_kit` keeps its grammar (`begin` / `row` / `carousel` / `detail` /
`headline` / `marquee`) and the Solarized theme. The **metrics** (`W`, `H`,
`ROW_H`, `STATUS_H`, font sizes, chevron geometry) move out of hardcoded
240×135 constants into a **compile-time board profile** supplied by the HAL.
Tool `draw()` code does not change. On the Tab5: chevrons become tap
targets; the keyboard makes **text-entry fields** and a **command palette /
on-device terminal** first-class (type `recall rolling code`, `run ble.scan
on <station>`, `stations`). Multi-pane / preview layouts are added as *new*
kit primitives only for the tools that need them (camera preview, terminal);
the menu/tool screens stay one-focus.

### 5. Dual-core tool execution

The WS link, the UI, and `dispatch` run on core 0. A **blocking tool body**
(radio scan, camera capture, pcap drain, CAN sniff window) runs as a
FreeRTOS task on core 1; its result is marshalled back to the dispatcher.
This is the platform-level fix for the StickS3's "blocking `run()` kills the
link" bug — designed in, not patched per tool. The Stick stays cooperative
(its constraint is documented in `ws-transport.md`).

### 6. New toolsets (scoped by hardware; radio ones gated on §2)

| Toolset | Hardware | Tools (initial) |
|---|---|---|
| `camera` | MIPI-CSI | photo → strike w/ thumbnail; QR / barcode decode; motion-triggered capture; timelapse |
| `thread` / `ieee802154` | C6 802.15.4 | Zigbee device scan; Thread / Matter commissioning probe; 15.4 promiscuous capture |
| `usbhost` | USB HS host | device enumeration + descriptor dump; USB mass-storage read (file loot); passive HID capture |
| `canbus` / `rs485` | TJA1051 / M-Bus | CAN sniff + replay; Modbus-RTU probe |
| `wifi` / `ble` | C6 | ported from the Stick **iff** monitor mode works over `esp_wifi_remote`; otherwise `wifi.scan` / `ble.scan` only |

Each new toolset ships a living `docs/mcp/<toolset>.md` spec (as the
existing ones do). Every tool obeys ADR-0001: capture / scan / decode /
replay, targeted, off by default, no untargeted-DoS headline. The camera is
a **documented capture tool** — no covert-surveillance framing.

### 7. Store-and-forward

The Tab5 has microSD, so it **spools strikes to disk when the collector is
unreachable** and forwards on reconnect — the SD-equipped station ADR-0009
§4 anticipated. It can also relay for Stick-class stations that cannot reach
the collector directly. The spool format is the strike envelope, one JSON
object per line; the collector de-dupes on strike `id`.

### 8. Fleet role

The Tab5 is three things at once: a **station** (declares
`camera` / `ieee802154` / `usbhost` / …), the **operator console** (big
touch screen + keyboard — renders a fleet view and the on-device terminal
from §4), and a **store-and-forward relay** (§7). It subsumes the
Cardputer-ADV "keyboard terminal" role sketched in ADR-0007; the Cardputer
becomes optional.

## Alternatives considered

- **One firmware, both boards, `#ifdef`.** Rejected: the P4 toolchain,
  `esp_wifi_remote`, the touch + keyboard input model, and the display
  scale diverge enough that a single tree would be mostly `#ifdef`. The
  shared surface is also about to move (§3, §4) — freezing it into a
  library or a shared tree now is premature.
- **Extract `sferic-core` before starting the Tab5.** Rejected for the same
  reason — one consumer, unstable surface. Copy first, extract once both are
  real.
- **Touch-only UI, ignore the keyboard.** Rejected: the operator has the
  keyboard, and it is what turns the Tab5 from a tablet into a console
  (terminal, text entry, shortcuts).
- **Single-core cooperative like the Stick.** Rejected: the Stick already
  demonstrated blocking `run()` breaks the uplink; the P4's second core
  exists — use it.
- **Assume C6 radio parity and port the whole `wifi.*` / `ble.*` catalog.**
  Rejected: monitor mode over `esp_wifi_remote` is unproven; scope follows
  the spike (§2).
- **ESP-IDF from the start (skip Arduino/M5Unified).** Deferred to the
  spike: use M5Unified if its P4 support is usable (keeps the shared UI/HAL
  code closest to the Stick), fall back to ESP-IDF + M5GFX only if it is not.

## Consequences

- **First work item is the bring-up spike (§2).** Toolset design waits on
  its monitor-mode result. The operator flashes it.
- **The StickS3 gets the HAL nav-event refactor (§3)** — small,
  backward-compatible, done before/alongside Tab5 work so touch + keyboard
  plug into a proven seam.
- `ui_kit` gains a board-profile metrics seam (§4); **no tool `draw()`
  changes** on either board.
- `m5stack-lab/sferic-tab5` is created by copying the shared sources; a
  `sferic-core` extraction is a future ADR.
- New `docs/mcp/{camera,thread,usbhost,canbus}.md` living specs, one per new
  toolset, written alongside the code.
- **Collector: no changes needed to accept a Tab5.** Toolset ids are
  strings; capability routing, id de-dup, and the WS listener already
  handle a new device type. `sferic-fake-station --toolsets
  camera,thread,usbhost` develops the collector side before the board is
  flashed. A dashboard tile per new toolset accent (camera / thread /
  usbhost / canbus) is a later cosmetic pass.
- The dual-core execution model (§5) is Tab5-specific; the Stick's
  cooperative constraint stays documented, not fixed.
- A change that drops or redefines a committed capability here is a
  superseding ADR.

## Update 2026-09-03 — bring-up spike results (`sferic-tab5/SPIKE-RESULTS.md`)

The spike (§2) ran on the physical board. Outcomes that change the plan:

- **Toolchain: confirmed.** pioarduino platform `54.03.21` + M5Unified +
  arduino-esp32 3.2.1 builds and flashes for the P4. Needs PlatformIO Core
  ≥ 6.2 on Python ≥ 3.10 (isolated from the StickS3's 3.9 pio). Display
  (M5GFX autodetect / ST7121 / 1280×720), touch (GT911-class), IMU
  (BMI270), 32 MB PSRAM all work with no driver work → **§3/§4 unblocked as
  planned.**

- **C6 radio: narrower than §6 assumed.** `WiFi.setPins(12,13,11,10,9,8,15)`
  (the Tab5's SDIO2→C6 wiring) brings `esp_hosted` up; **STA connect + scan
  work.** But over `esp_hosted`:
  - `esp_wifi_set_promiscuous()` → `ESP_ERR_NOT_SUPPORTED`
  - `esp_wifi_80211_tx()` → `ESP_ERR_NOT_SUPPORTED`
  - BLE and OpenThread/802.15.4 are **compiled out** of the stock P4 libs
    (`CONFIG_BT_ENABLED`, `CONFIG_OPENTHREAD_ENABLED` not set).

  So with stock tooling the Tab5's radio is **`wifi.scan` + `wifi.portal`
  only.** The monitor-class `wifi.*` tools, `ble.*`, and `thread` /
  `ieee802154` need a custom Arduino-as-IDF-component build — and monitor /
  raw-TX are `esp_hosted` *protocol* limits, so they likely stay
  unavailable even then.

**Revised §6 toolset scope for the Tab5:**

| Toolset | Revised status |
|---|---|
| `camera` | unchanged — the Tab5's headline capability |
| `usbhost` | unchanged |
| `canbus` / `rs485` | unchanged |
| `wifi` | **`wifi.scan` + `wifi.portal` only** (no monitor/probe/handshake/deauth — those stay StickS3-only) |
| `ble` | **dropped from initial scope** — needs custom firmware; revisit if BLE-scan value justifies it |
| `thread` / `ieee802154` | **dropped from initial scope** — needs custom firmware and may not be transportable over `esp_hosted` at all; a separate spike if pursued |

This does not change the ADR's direction — it sharpens it. The Tab5 was
always the "rich touch field station" (§8), not a radio node; the spike
confirms the radio story is thin and the **camera / USB / screen+keyboard /
compute** are the real reasons it is in the fleet. The fleet model holds:
the StickS3 (+ RF Pack) sniffs and emits; the Tab5 scans, sees, and is the
operator console.

Open spike items: microSD (on-chip LDO power config), the keyboard
connection, camera bring-up.
