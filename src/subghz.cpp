#include "subghz.h"

#ifdef SFERIC_RF_PACK

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cmath>
#include <cstdio>

#include "hal.h"
#include "rf_pack.h"
#include "theme.h"
#include "ui_kit.h"

// UNVERIFIED — the CC1101 register values below are the widely-used SmartRC /
// rc-switch "OOK 433.92 MHz, async serial" config. They compile and the
// screens are reachable; capture/replay/decode are not claimed to work until
// confirmed on the board (docs/hardware/rf-pack-s3.md). RSSI decode + the
// frequency word are likewise from the datasheet.

namespace {
constexpr int kTitleH = 18;

// CC1101 config registers used by the scan sweep
constexpr uint8_t REG_FREQ2 = 0x0D;
constexpr uint8_t REG_FREQ1 = 0x0E;
constexpr uint8_t REG_FREQ0 = 0x0F;
constexpr uint8_t REG_RSSI = 0x34;   // status space
constexpr uint8_t STROBE_SRX = 0x34;
constexpr uint8_t STROBE_STX = 0x35;
constexpr uint8_t STROBE_SIDLE = 0x36;
constexpr uint8_t STROBE_SRES = 0x30;
constexpr uint8_t STROBE_SFRX = 0x3A;
constexpr uint8_t STROBE_SFTX = 0x3B;

// 26 MHz crystal. FREQ = f_carrier * 2^16 / f_xosc.
constexpr double kFxosc = 26'000'000.0;

// Sweep 433.05 .. 434.79 MHz (EU 433 ISM), kSteps points.
constexpr double kBandLoHz = 433'050'000.0;
constexpr double kBandHiHz = 434'790'000.0;

constexpr double kDefaultMhz = 433.92;

// -------- OOK config (SmartRC / rc-switch, async serial on GDO0) -----------
struct RegVal {
  uint8_t addr, val;
};
constexpr RegVal kOok[] = {
    {0x00, 0x2E},  // IOCFG2  — hi-Z (unused)
    {0x02, 0x0D},  // IOCFG0  — async serial data (out in RX, in in TX)
    {0x03, 0x47},  // FIFOTHR
    {0x08, 0x30},  // PKTCTRL0 — async serial mode, no CRC, infinite length
    {0x0B, 0x06},  // FSCTRL1
    {0x0C, 0x00},  // FSCTRL0
    {0x0D, 0x10}, {0x0E, 0xB0}, {0x0F, 0x71},  // FREQ2/1/0 ≈ 433.92 MHz
    {0x10, 0x57},  // MDMCFG4 — RX filter BW
    {0x11, 0xC4},  // MDMCFG3 — data-rate mantissa (async clock)
    {0x12, 0x30},  // MDMCFG2 — ASK/OOK, no preamble/sync
    {0x13, 0x22},  // MDMCFG1
    {0x14, 0xF8},  // MDMCFG0
    {0x15, 0x15},  // DEVIATN (unused for OOK)
    {0x17, 0x30},  // MCSM1 — CCA off, stay put
    {0x18, 0x18},  // MCSM0 — auto-cal on IDLE→RX/TX
    {0x19, 0x16},  // FOCCFG
    {0x1B, 0x07},  // AGCCTRL2 — max gain, magn target
    {0x1C, 0x00},  // AGCCTRL1
    {0x1D, 0x91},  // AGCCTRL0 — OOK decision / hysteresis
    {0x21, 0x56},  // FREND1
    {0x22, 0x11},  // FREND0 — PA_POWER index 1 for OOK keying
    {0x23, 0xE9}, {0x24, 0x2A}, {0x25, 0x00}, {0x26, 0x1F},  // FSCAL3..0
    {0x2C, 0x81}, {0x2D, 0x35}, {0x2E, 0x09},  // TEST2..0
};
constexpr uint8_t kPaOok[] = {0x00, 0xC0};  // [0] off, [1] ≈ +10 dBm

void setFreqMhz(double mhz) {
  const uint32_t w = (uint32_t)llround(mhz * 1e6 * 65536.0 / kFxosc);
  rfpack::cc1101WriteReg(REG_FREQ2, (w >> 16) & 0xFF);
  rfpack::cc1101WriteReg(REG_FREQ1, (w >> 8) & 0xFF);
  rfpack::cc1101WriteReg(REG_FREQ0, w & 0xFF);
}

void applyOok(double mhz) {
  rfpack::cc1101Strobe(STROBE_SRES);
  delay(5);
  for (const auto& r : kOok) rfpack::cc1101WriteReg(r.addr, r.val);
  rfpack::cc1101WritePaTable(kPaOok, sizeof(kPaOok));
  setFreqMhz(mhz > 0 ? mhz : kDefaultMhz);
  rfpack::cc1101Strobe(STROBE_SIDLE);
}

// One OOK burst → mark/space µs into out[] ([0] = mark). Blocks up to
// budgetMs. Returns edge count; 0 = nothing keyed.
size_t captureOok(uint16_t* out, size_t maxN, uint32_t budgetMs) {
  rfpack::cc1101Strobe(STROBE_SIDLE);
  rfpack::cc1101Strobe(STROBE_SRX);
  delayMicroseconds(800);

  const uint32_t deadline = millis() + budgetMs;
  while (millis() < deadline && rfpack::cc1101Gdo0() != 0) {
  }
  if (millis() >= deadline) {
    rfpack::cc1101Strobe(STROBE_SIDLE);
    return 0;
  }

  uint32_t tEdge = micros();
  int last = 1;
  size_t n = 0;
  while (n < maxN) {
    const int cur = rfpack::cc1101Gdo0();
    const uint32_t now = micros();
    if (cur != last) {
      uint32_t d = now - tEdge;
      tEdge = now;
      last = cur;
      out[n++] = d > 65535 ? 65535 : (uint16_t)d;
    } else if (now - tEdge > 12000) {
      break;  // 12 ms of silence ends the burst
    }
    if (millis() >= deadline) break;
  }
  rfpack::cc1101Strobe(STROBE_SIDLE);
  return n;
}

// PT2262 / EV1527 class (rc-switch protocol 1): sync 1:31, '0' 1:3, '1' 3:1,
// 24 data bits. Returns 24 and fills value/te on a frame, else 0.
int decodePt2262(const uint16_t* t, size_t n, uint32_t& value, uint16_t& teOut) {
  if (n < 50) return 0;
  uint32_t te = 65535;
  for (size_t i = 0; i + 1 < n && i < 60; i += 2)
    if (t[i] > 60 && t[i] < te) te = t[i];
  if (te == 65535 || te < 60) return 0;
  const uint32_t lo = te / 2;

  for (size_t start = 0; start + 50 <= n; start += 2) {
    uint32_t v = 0;
    int bits = 0;
    bool ok = true;
    for (int b = 0; b < 24; ++b) {
      const uint32_t mark = t[start + b * 2];
      const uint32_t space = t[start + b * 2 + 1];
      const bool one = mark > space;
      const uint32_t big = one ? mark : space;
      const uint32_t small = one ? space : mark;
      if (big < 2 * small || small < lo || small > te * 2) {
        ok = false;
        break;
      }
      v = (v << 1) | (one ? 1u : 0u);
      ++bits;
    }
    if (!ok || bits != 24) continue;
    const size_t gapIdx = start + 48;
    if (gapIdx < n && t[gapIdx] > te * 20) {
      value = v;
      teOut = (uint16_t)te;
      return 24;
    }
  }
  return 0;
}

bool txOok(const uint16_t* t, size_t n, double mhz, int repeats) {
  if (!n) return false;
  applyOok(mhz);
  rfpack::cc1101Gdo0Output(true);
  rfpack::cc1101Gdo0Write(false);
  rfpack::cc1101Strobe(STROBE_STX);
  delayMicroseconds(800);

  if (repeats < 1) repeats = 1;
  if (repeats > 20) repeats = 20;
  for (int r = 0; r < repeats; ++r) {
    bool level = true;  // t[0] = mark
    for (size_t i = 0; i < n; ++i) {
      rfpack::cc1101Gdo0Write(level);
      delayMicroseconds(t[i]);
      level = !level;
    }
    rfpack::cc1101Gdo0Write(false);
    delayMicroseconds(8000);  // inter-frame gap
  }
  rfpack::cc1101Strobe(STROBE_SIDLE);
  rfpack::cc1101Gdo0Output(false);
  return true;
}

// ---- scan-only helpers (unchanged) ----
void tuneHz(double hz) {
  const uint32_t word = (uint32_t)(hz * 65536.0 / kFxosc);
  rfpack::cc1101Strobe(STROBE_SIDLE);
  rfpack::cc1101WriteReg(REG_FREQ2, (word >> 16) & 0xFF);
  rfpack::cc1101WriteReg(REG_FREQ1, (word >> 8) & 0xFF);
  rfpack::cc1101WriteReg(REG_FREQ0, word & 0xFF);
  rfpack::cc1101Strobe(STROBE_SRX);
}

int8_t readRssiDbm() {
  const uint8_t raw = rfpack::cc1101ReadStatus(REG_RSSI);
  const int r = raw >= 128 ? (raw - 256) : raw;
  return (int8_t)(r / 2 - 74);  // datasheet offset for 433 MHz
}

double stepHz(int i) {
  return kBandLoHz + (kBandHiHz - kBandLoHz) * i / (SubGhzScanScreen::kSteps - 1);
}

uint32_t paramMs(const std::string& p, uint32_t def, uint32_t lo, uint32_t hi) {
  JsonDocument d;
  if (deserializeJson(d, p) != DeserializationError::Ok) return def;
  if (!d["ms"].is<uint32_t>()) return def;
  const uint32_t ms = d["ms"];
  return ms < lo ? lo : (ms > hi ? hi : ms);
}

double paramMhz(const std::string& p) {
  JsonDocument d;
  if (deserializeJson(d, p) != DeserializationError::Ok) return kDefaultMhz;
  if (d["mhz"].is<float>()) return (double)d["mhz"];
  return kDefaultMhz;
}
}  // namespace

// ======================================================== scan screen ======

void SubGhzScanScreen::onEnter() {
  dirty_ = true;
  step_ = 0;
  peakIdx_ = 0;
  detected_ = rfpack::presence().cc1101 && rfpack::acquire(rfpack::Radio::SubGhz);
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SRES);  // power-on defaults — the RSSI math
    delay(5);                           // assumes them (an OOK config skews it)
  }
  for (auto& v : rssi_) v = -120;
}

void SubGhzScanScreen::onExit() {
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SIDLE);
    rfpack::release();
  }
}

void SubGhzScanScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    step_ = 0;
    peakIdx_ = 0;
    for (auto& v : rssi_) v = -120;
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  if (detected_ && now - lastStep_ > 30) {
    lastStep_ = now;
    tuneHz(stepHz(step_));
    delayMicroseconds(400);  // PLL settle + RSSI valid
    rssi_[step_] = readRssiDbm();
    if (rssi_[step_] > rssi_[peakIdx_]) peakIdx_ = step_;
    step_ = (step_ + 1) % kSteps;
    dirty_ = true;
  }

  if (now - lastDraw_ > 120) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void SubGhzScanScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  kit::begin("433 Band Scan", toolsetAccent("subghz"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "CC1101 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  const int gx = 6, gy = kit::BODY_Y + 6;
  const int gw = kit::W - 12, gh = kit::BODY_H - 40;
  for (int i = 0; i < kSteps; ++i) {
    const int v = rssi_[i];  // ~ -120 .. -20 dBm
    int hgt = (v + 120) * gh / 100;
    if (hgt < 0) hgt = 0;
    if (hgt > gh) hgt = gh;
    const int bx = gx + i * gw / kSteps;
    const int bw = gw / kSteps - 1;
    g.fillRect(bx, gy + gh - hgt, bw, hgt,
               i == peakIdx_ ? th.warn : toolsetAccent("subghz"));
  }

  char l[40];
  std::snprintf(l, sizeof(l), "peak %.2f MHz", stepHz(peakIdx_) / 1e6);
  g.setTextSize(2);
  g.setTextColor(th.fg, th.bg);
  g.setCursor(8, gy + gh + 6);
  g.print(l);
  std::snprintf(l, sizeof(l), "%d dBm", rssi_[peakIdx_]);
  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(8, gy + gh + 24);
  g.print(l);

  kit::end("A restart   B- back");
}

std::string subghzScanJson(const std::string& params) {
  if (!rfpack::presence().cc1101) return R"({"error":"radio_absent"})";
  if (!rfpack::acquire(rfpack::Radio::SubGhz))
    return R"({"error":"radio_busy"})";

  const uint32_t budget = paramMs(params, 4000, 500, 15000);
  rfpack::cc1101Strobe(STROBE_SRES);  // power-on defaults for the RSSI math
  delay(5);

  JsonDocument d;
  d["band"] = "433";
  JsonArray pts = d["points"].to<JsonArray>();
  int peak = 0;
  const uint32_t start = millis();
  for (int i = 0; i < SubGhzScanScreen::kSteps && millis() - start < budget;
       ++i) {
    tuneHz(stepHz(i));
    delayMicroseconds(400);
    const int8_t r = readRssiDbm();
    JsonObject o = pts.add<JsonObject>();
    o["mhz"] = stepHz(i) / 1e6;
    o["rssi"] = r;
    if (r > (int)pts[peak]["rssi"]) peak = i;
  }
  rfpack::cc1101Strobe(STROBE_SIDLE);
  rfpack::release();

  d["peak"]["mhz"] = stepHz(peak) / 1e6;
  d["peak"]["rssi"] = (int)pts[peak]["rssi"];
  std::string out;
  serializeJson(d, out);
  return out;
}

// =================================================== capture / replay ======

void SubGhzLearnScreen::onEnter() {
  dirty_ = true;
  detected_ =
      rfpack::presence().cc1101 && rfpack::acquire(rfpack::Radio::SubGhz);
  if (detected_) applyOok(kDefaultMhz);
}

void SubGhzLearnScreen::onExit() {
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SIDLE);
    rfpack::release();
  }
}

void SubGhzLearnScreen::arm() {
  strike_ = Strike{};
  mode_ = Mode::Listening;
  dirty_ = true;
  Serial.println("[subghz] armed - key an OOK remote");
}

void SubGhzLearnScreen::replay() {
  if (mode_ != Mode::Captured || strike_.raw.empty()) return;
  const bool sent =
      txOok(strike_.raw.data(), strike_.raw.size(), kDefaultMhz, 5);
  Serial.printf("[subghz] replayed: %s\n", sent ? strike_.summary : "nothing");
  dirty_ = true;
}

void SubGhzLearnScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }

  const bool wasArmed = replayArm_.armed;
  const bool trigB = (b == BtnEvent::Click && mode_ == Mode::Captured);
  if (replayArm_.update(trigB, a == BtnEvent::Click, hal.nowMs())) {
    replay();
  } else if (a == BtnEvent::Click && !wasArmed && detected_) {
    arm();
  }
  if (trigB || wasArmed != replayArm_.armed) dirty_ = true;

  if (mode_ == Mode::Listening && detected_) {
    static uint16_t buf[kSubGhzRawMax];
    const size_t n = captureOok(buf, kSubGhzRawMax, 400);
    if (n >= 16) {
      strike_.raw.assign(buf, buf + n);
      strike_.bits =
          decodePt2262(buf, n, strike_.code, strike_.te) ? 24 : 0;
      if (strike_.bits)
        std::snprintf(strike_.summary, sizeof(strike_.summary),
                      "PT2262  0x%06lX / %ub", (unsigned long)strike_.code,
                      strike_.bits);
      else
        std::snprintf(strike_.summary, sizeof(strike_.summary), "RAW  %u edges",
                      (unsigned)strike_.raw.size());
      mode_ = Mode::Captured;
      dirty_ = true;
      Serial.printf("[subghz] captured: %s\n", strike_.summary);
    }
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void SubGhzLearnScreen::draw() {
  const Theme& th = theme();
  kit::begin("Learn / Replay", toolsetAccent("subghz"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "CC1101 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  const char* head = mode_ == Mode::Idle        ? "IDLE"
                     : mode_ == Mode::Listening ? "LISTEN"
                     : replayArm_.armed         ? "ARMED"
                                                : "CAUGHT";
  const uint16_t hc = mode_ == Mode::Listening ? th.warn
                      : replayArm_.armed       ? th.bad
                      : mode_ == Mode::Captured ? th.ok
                                                : th.fgDim;
  kit::headline(head, hc);

  if (mode_ == Mode::Captured) {
    kit::line(0, strike_.summary);
    char l[32];
    std::snprintf(l, sizeof(l), "%u edges  te %uus",
                  (unsigned)strike_.raw.size(), (unsigned)strike_.te);
    kit::line(1, l, th.fgDim);
  } else if (mode_ == Mode::Idle) {
    kit::line(0, "A: arm, key a remote", th.fgDim);
  }

  kit::end(mode_ != Mode::Captured  ? "A arm   B- back"
           : replayArm_.armed       ? "B send   A cancel   B- back"
                                    : "A recapture   B arm   B- back");
}

std::string subghzCaptureJson(const std::string& params) {
  if (!rfpack::presence().cc1101) return R"({"error":"radio_absent"})";
  if (!rfpack::acquire(rfpack::Radio::SubGhz))
    return R"({"error":"radio_busy"})";

  const uint32_t budget = paramMs(params, 5000, 500, 20000);
  const double mhz = paramMhz(params);
  applyOok(mhz);

  static uint16_t buf[kSubGhzRawMax];
  const uint32_t t0 = millis();
  size_t n = 0;
  while (millis() - t0 < budget && n < 16) {
    n = captureOok(buf, kSubGhzRawMax, 400);
  }
  rfpack::cc1101Strobe(STROBE_SIDLE);
  rfpack::release();

  if (n < 16) return R"({"error":"timeout"})";

  uint32_t dur = 0;
  for (size_t i = 0; i < n; ++i) dur += buf[i];

  uint32_t value = 0;
  uint16_t te = 0;
  const int bits = decodePt2262(buf, n, value, te);

  JsonDocument d;
  d["v"] = 1;
  d["toolset"] = "subghz";
  d["kind"] = "ook";
  d["ts"] = String("t+") + String(millis());
  JsonObject meta = d["meta"].to<JsonObject>();
  meta["frequency_hz"] = (uint32_t)llround(mhz * 1e6);
  meta["modulation"] = "OOK";
  meta["duration_us"] = dur;
  meta["edge_count"] = (uint32_t)n;
  JsonObject raw = d["payload"]["raw"].to<JsonObject>();
  raw["encoding"] = "us-timings";
  JsonArray data = raw["data"].to<JsonArray>();
  for (size_t i = 0; i < n; ++i) data.add(buf[i]);
  if (bits) {
    JsonObject dec = d["payload"]["decoded"].to<JsonObject>();
    dec["protocol"] = "PT2262";
    dec["bits"] = bits;
    char v[12];
    std::snprintf(v, sizeof(v), "0x%06lX", (unsigned long)value);
    dec["value"] = v;
    dec["te_us"] = te;
  }
  std::string out;
  serializeJson(d, out);
  return out;
}

std::string subghzReplayEmit(const std::string& params) {
  if (!rfpack::presence().cc1101) return R"({"error":"radio_absent"})";
  if (!rfpack::acquire(rfpack::Radio::SubGhz))
    return R"({"error":"radio_busy"})";

  JsonDocument p;
  const bool okJson = deserializeJson(p, params) == DeserializationError::Ok;
  JsonArrayConst arr;
  if (okJson) {
    if (p["raw"].is<JsonArrayConst>())
      arr = p["raw"].as<JsonArrayConst>();
    else if (p["payload"]["raw"]["data"].is<JsonArrayConst>())
      arr = p["payload"]["raw"]["data"].as<JsonArrayConst>();
  }
  if (arr.isNull() || arr.size() < 2) {
    rfpack::release();
    return R"({"error":"bad_params"})";
  }

  std::vector<uint16_t> t;
  t.reserve(arr.size());
  for (JsonVariantConst v : arr) {
    uint32_t us = v.as<uint32_t>();
    t.push_back(us > 65535 ? 65535 : (uint16_t)us);
  }
  const double mhz = p["mhz"].is<float>() ? (double)p["mhz"] : kDefaultMhz;
  const int repeats = p["repeats"].is<int>() ? (int)p["repeats"] : 5;

  const bool sent = txOok(t.data(), t.size(), mhz, repeats);
  rfpack::release();

  JsonDocument d;
  d["sent"] = sent;
  d["frames"] = repeats < 1 ? 1 : (repeats > 20 ? 20 : repeats);
  std::string out;
  serializeJson(d, out);
  return out;
}

// ============================================================ decode =======

namespace {
constexpr uint32_t kDecodeSliceMs = 250;

bool decodeSlice(uint32_t& value, uint16_t& bits, uint16_t& te) {
  static uint16_t buf[kSubGhzRawMax];
  const size_t n = captureOok(buf, kSubGhzRawMax, kDecodeSliceMs);
  if (n < 50) return false;
  uint32_t v = 0;
  uint16_t t = 0;
  if (!decodePt2262(buf, n, v, t)) return false;
  value = v;
  bits = 24;
  te = t;
  return true;
}
}  // namespace

void SubGhzDecodeScreen::onEnter() {
  dirty_ = true;
  hist_.clear();
  detected_ =
      rfpack::presence().cc1101 && rfpack::acquire(rfpack::Radio::SubGhz);
  if (detected_) applyOok(kDefaultMhz);
}

void SubGhzDecodeScreen::onExit() {
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SIDLE);
    rfpack::release();
  }
}

void SubGhzDecodeScreen::tick() {
  const BtnEvent b = hal.event(Btn::B);
  (void)hal.event(Btn::A);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }

  if (detected_) {
    uint32_t value = 0;
    uint16_t bits = 0, te = 0;
    if (decodeSlice(value, bits, te)) {
      char l[40];
      std::snprintf(l, sizeof(l), "PT2262 0x%06lX  te%u", (unsigned long)value,
                    (unsigned)te);
      hist_.insert(hist_.begin(), l);
      if (hist_.size() > 6) hist_.pop_back();
      lastHit_ = hal.nowMs();
      dirty_ = true;
      Serial.printf("[subghz] decode %s\n", l);
    }
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void SubGhzDecodeScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  kit::begin("Protocol Decode", toolsetAccent("subghz"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "CC1101 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  int y = kit::BODY_Y + 4;
  g.setTextSize(1);
  if (hist_.empty()) {
    g.setTextColor(th.fgDim, th.bg);
    g.setCursor(8, y);
    g.print("listening 433.92 OOK...");
  } else {
    for (size_t i = 0; i < hist_.size(); ++i) {
      g.setTextColor(i == 0 ? th.fg : th.fgDim, th.bg);
      g.setCursor(8, y);
      g.print(hist_[i].c_str());
      y += 12;
    }
  }
  kit::end("B- back");
}

bool subghzDecodeStreamStart(const std::string& params) {
  if (!rfpack::presence().cc1101) return false;
  if (!rfpack::acquire(rfpack::Radio::SubGhz)) return false;
  applyOok(paramMhz(params));
  return true;
}

void subghzDecodeStreamPoll(const StrikeEmit& emit) {
  uint32_t value = 0;
  uint16_t bits = 0, te = 0;
  if (!decodeSlice(value, bits, te)) return;
  JsonDocument d;
  d["protocol"] = "PT2262";
  d["bits"] = bits;
  char v[12];
  std::snprintf(v, sizeof(v), "0x%06lX", (unsigned long)value);
  d["value"] = v;
  d["te_us"] = te;
  emit("code", d);
}

void subghzDecodeStreamStop() {
  rfpack::cc1101Strobe(STROBE_SIDLE);
  rfpack::release();
}

// ============================================================ fox hunt =====

namespace {
constexpr double kFoxPresets[] = {433.92, 315.0, 868.35, 915.0, 433.42};
constexpr int kFoxN = sizeof(kFoxPresets) / sizeof(kFoxPresets[0]);
}  // namespace

void SubGhzFoxhuntScreen::onEnter() {
  dirty_ = true;
  cur_ = peak_ = -120;
  head_ = 0;
  for (auto& v : hist_) v = -120;
  detected_ =
      rfpack::presence().cc1101 && rfpack::acquire(rfpack::Radio::SubGhz);
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SRES);
    delay(5);
    tuneHz(kFoxPresets[preset_] * 1e6);
  }
}

void SubGhzFoxhuntScreen::onExit() {
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SIDLE);
    rfpack::release();
  }
}

void SubGhzFoxhuntScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click && detected_) {
    preset_ = (preset_ + 1) % kFoxN;
    cur_ = peak_ = -120;
    for (auto& v : hist_) v = -120;
    tuneHz(kFoxPresets[preset_] * 1e6);
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  if (detected_ && now - lastSample_ >= 15) {
    lastSample_ = now;
    cur_ = readRssiDbm();
    hist_[head_] = cur_;
    head_ = (head_ + 1) % kGraph;
    if (cur_ > peak_) peak_ = cur_;
    dirty_ = true;
  }

  if (now - lastDraw_ > 70 && dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void SubGhzFoxhuntScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  char t[28];
  std::snprintf(t, sizeof(t), "Fox Hunt  %.2f", kFoxPresets[preset_]);
  kit::begin(t, toolsetAccent("subghz"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "CC1101 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  char l[24];
  std::snprintf(l, sizeof(l), "%d dBm", cur_);
  const uint16_t c = cur_ > -55 ? th.bad : cur_ > -80 ? th.warn : th.fgDim;
  kit::bigValue(l, c);

  const int gx = 4, gw = kGraph;
  const int gy = kit::BODY_Y + 44;
  const int gh = kit::BODY_H - 52;
  g.drawRect(gx, gy, gw + 2, gh, th.line);
  for (int x = 0; x < gw; ++x) {
    const int v = hist_[(head_ + x) % kGraph];  // oldest → newest, L → R
    int hh = (v + 110) * gh / 70;
    if (hh < 0) hh = 0;
    if (hh > gh - 2) hh = gh - 2;
    if (hh > 0)
      g.drawFastVLine(gx + 1 + x, gy + gh - 1 - hh, hh, toolsetAccent("subghz"));
  }
  std::snprintf(l, sizeof(l), "peak %d", peak_);
  g.setTextSize(1);
  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(gx + gw - 46, gy + 2);
  g.print(l);

  kit::end("A freq   B- back");
}

std::string subghzFoxhuntJson(const std::string& params) {
  if (!rfpack::presence().cc1101) return R"({"error":"radio_absent"})";
  if (!rfpack::acquire(rfpack::Radio::SubGhz))
    return R"({"error":"radio_busy"})";

  const uint32_t budget = paramMs(params, 3000, 300, 15000);
  const double mhz = paramMhz(params);
  rfpack::cc1101Strobe(STROBE_SRES);
  delay(5);
  tuneHz(mhz * 1e6);
  delayMicroseconds(600);

  int now = -120, lo = 0, hi = -120;
  long sum = 0;
  uint32_t n = 0;
  const uint32_t start = millis();
  while (millis() - start < budget) {
    now = readRssiDbm();
    if (now < lo) lo = now;
    if (now > hi) hi = now;
    sum += now;
    ++n;
    delayMicroseconds(1500);
  }
  rfpack::cc1101Strobe(STROBE_SIDLE);
  rfpack::release();

  JsonDocument d;
  d["mhz"] = mhz;
  d["samples"] = n;
  d["rssi"]["now"] = now;
  d["rssi"]["min"] = n ? lo : 0;
  d["rssi"]["max"] = hi;
  d["rssi"]["mean"] = n ? (int)(sum / (long)n) : 0;
  std::string out;
  serializeJson(d, out);
  return out;
}

// =========================================================== waterfall =====

namespace {
double wfBinHz(int i) {
  return kBandLoHz +
         (kBandHiHz - kBandLoHz) * i / (SubGhzWaterfallScreen::kBins - 1);
}

// dBm (~ -120..-40) → heat colour, `gain` steepens the low end.
uint16_t heat(int dbm, uint8_t gain) {
  int v = (dbm + 120) * gain;  // 0..~(80*gain)
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  if (v < 20) return rgb565(0, 0, 40 + v);          // near-black → blue
  if (v < 45) return rgb565(0, (v - 20) * 8, 120);  // blue → cyan
  if (v < 65) return rgb565((v - 45) * 10, 200, 120 - (v - 45) * 6);  // → green
  if (v < 85) return rgb565(200 + (v - 65) * 2, 200 - (v - 65) * 4, 0);  // → yel
  return rgb565(255, (100 - v) * 12, 0);            // → red
}
}  // namespace

void SubGhzWaterfallScreen::onEnter() {
  dirty_ = true;
  head_ = 0;
  bin_ = 0;
  for (auto& r : hist_)
    for (auto& c : r) c = -120;
  for (auto& c : row_) c = -120;
  detected_ =
      rfpack::presence().cc1101 && rfpack::acquire(rfpack::Radio::SubGhz);
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SRES);
    delay(5);
  }
}

void SubGhzWaterfallScreen::onExit() {
  if (detected_) {
    rfpack::cc1101Strobe(STROBE_SIDLE);
    rfpack::release();
  }
}

void SubGhzWaterfallScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    gain_ = gain_ >= 3 ? 1 : gain_ + 1;
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  if (detected_ && now - lastStep_ > 20) {
    lastStep_ = now;
    tuneHz(wfBinHz(bin_));
    delayMicroseconds(400);
    row_[bin_] = readRssiDbm();
    if (++bin_ >= kBins) {
      bin_ = 0;
      head_ = (head_ + 1) % kRows;
      for (int i = 0; i < kBins; ++i) hist_[head_][i] = row_[i];
      dirty_ = true;
    }
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void SubGhzWaterfallScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  kit::begin("433 Waterfall", toolsetAccent("subghz"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "CC1101 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  const int bw = kit::W / kBins;
  const int rh = kit::BODY_H / kRows > 0 ? kit::BODY_H / kRows : 1;
  g.startWrite();
  for (int age = 0; age < kRows; ++age) {
    const int hr = (head_ - age + kRows * 4) % kRows;
    const int y = kit::BODY_Y + kit::BODY_H - (age + 1) * rh;
    if (y < kit::BODY_Y) break;
    for (int x = 0; x < kBins; ++x)
      g.fillRect(x * bw, y, bw, rh, heat(hist_[hr][x], gain_));
  }
  g.endWrite();

  char l[24];
  std::snprintf(l, sizeof(l), "433.1-434.8  x%u", gain_);
  g.setTextSize(1);
  g.setTextColor(th.fg, th.bg);
  g.setCursor(4, kit::BODY_Y + 1);
  g.print(l);

  kit::end("A contrast   B- back");
}

#endif  // SFERIC_RF_PACK
