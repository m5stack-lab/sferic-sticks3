#include "subghz.h"

#ifdef SFERIC_RF_PACK

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdio>

#include "hal.h"
#include "rf_pack.h"
#include "theme.h"
#include "ui_kit.h"

// UNVERIFIED — CC1101 register addresses / RSSI decode / frequency word are
// from the datasheet, not bench-checked. First bring-up: confirm against
// docs/hardware/rf-pack-s3.md, then delete this banner.

namespace {
constexpr int kTitleH = 18;

// CC1101 config registers
constexpr uint8_t REG_FREQ2 = 0x0D;
constexpr uint8_t REG_FREQ1 = 0x0E;
constexpr uint8_t REG_FREQ0 = 0x0F;
constexpr uint8_t REG_RSSI = 0x34;   // status space
constexpr uint8_t STROBE_SRX = 0x34;
constexpr uint8_t STROBE_SIDLE = 0x36;

// 26 MHz crystal. FREQ = f_carrier * 2^16 / f_xosc.
constexpr double kFxosc = 26'000'000.0;

// Sweep 433.05 .. 434.79 MHz (EU 433 ISM), kSteps points.
constexpr double kBandLoHz = 433'050'000.0;
constexpr double kBandHiHz = 434'790'000.0;

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
}  // namespace

void SubGhzScanScreen::onEnter() {
  dirty_ = true;
  step_ = 0;
  peakIdx_ = 0;
  detected_ = rfpack::presence().cc1101 && rfpack::acquire(rfpack::Radio::SubGhz);
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
    const int v = rssi_[i];                       // ~ -120 .. -20 dBm
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

// ---------------- headless ----------------

std::string subghzScanJson(const std::string& params) {
  if (!rfpack::presence().cc1101) return R"({"error":"radio_absent"})";
  if (!rfpack::acquire(rfpack::Radio::SubGhz))
    return R"({"error":"radio_busy"})";

  uint32_t budget = 4000;
  JsonDocument p;
  if (deserializeJson(p, params) == DeserializationError::Ok &&
      p["ms"].is<uint32_t>())
    budget = p["ms"];

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

#endif  // SFERIC_RF_PACK
