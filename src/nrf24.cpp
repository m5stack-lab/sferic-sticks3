#include "nrf24.h"

#ifdef SFERIC_RF_PACK

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdio>

#include "hal.h"
#include "rf_pack.h"
#include "theme.h"
#include "ui_kit.h"

// UNVERIFIED — nRF24L01+ register map / RPD sequence from the datasheet, not
// bench-checked. Confirm on first bring-up (docs/hardware/rf-pack-s3.md).

namespace {
constexpr int kTitleH = 18;

constexpr uint8_t REG_CONFIG = 0x00;
constexpr uint8_t REG_EN_AA = 0x01;
constexpr uint8_t REG_RF_CH = 0x05;
constexpr uint8_t REG_RF_SETUP = 0x06;
constexpr uint8_t REG_RPD = 0x09;  // bit0 = received-power-detector latch

// PWR_UP | PRIM_RX
constexpr uint8_t CONFIG_RX = 0x02 | 0x01;

void radioUp() {
  rfpack::nrf24Ce(false);
  rfpack::nrf24WriteReg(REG_CONFIG, CONFIG_RX);
  rfpack::nrf24WriteReg(REG_EN_AA, 0x00);      // no auto-ack
  rfpack::nrf24WriteReg(REG_RF_SETUP, 0x0F);   // 1 Mbps, 0 dBm, LNA on (approx)
  delay(2);                                    // Tpd2stby
}

void radioDown() {
  rfpack::nrf24Ce(false);
  rfpack::nrf24WriteReg(REG_CONFIG, 0x00);  // power down
}

// One channel: park, tune, listen a beat, sample RPD.
bool carrierOn(int ch) {
  rfpack::nrf24Ce(false);
  rfpack::nrf24WriteReg(REG_RF_CH, (uint8_t)ch);
  rfpack::nrf24Ce(true);
  delayMicroseconds(200);  // > 130us Tstby2a + settle
  rfpack::nrf24Ce(false);
  return rfpack::nrf24ReadReg(REG_RPD) & 0x01;
}
}  // namespace

void Nrf24ScanScreen::onEnter() {
  dirty_ = true;
  ch_ = 0;
  peakCh_ = 0;
  sweeps_ = 0;
  for (auto& v : hits_) v = 0;
  detected_ =
      rfpack::presence().nrf24 && rfpack::acquire(rfpack::Radio::Nrf24);
  if (detected_) radioUp();
}

void Nrf24ScanScreen::onExit() {
  if (detected_) {
    radioDown();
    rfpack::release();
  }
}

void Nrf24ScanScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    for (auto& v : hits_) v = 0;
    sweeps_ = 0;
    peakCh_ = 0;
    dirty_ = true;
  }

  if (detected_) {
    // a burst of channels per tick keeps the sweep lively
    for (int n = 0; n < 12; ++n) {
      if (carrierOn(ch_) && hits_[ch_] < 255) {
        if (++hits_[ch_] > hits_[peakCh_]) peakCh_ = ch_;
      } else if (hits_[ch_] > 0) {
        hits_[ch_]--;  // slow decay so the display tracks "now"
      }
      if (++ch_ >= kChannels) {
        ch_ = 0;
        sweeps_++;
      }
    }
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  if (now - lastDraw_ > 120) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void Nrf24ScanScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  char t[36];
  std::snprintf(t, sizeof(t), "2.4GHz Scan  x%lu", (unsigned long)sweeps_);
  kit::begin(t, toolsetAccent("nrf24"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "nRF24 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  const int gx = 4, gy = kit::BODY_Y + 6;
  const int gw = kit::W - 8, gh = kit::BODY_H - 40;
  for (int c = 0; c < kChannels; ++c) {
    int hgt = hits_[c] * gh / 16;
    if (hgt > gh) hgt = gh;
    if (hgt <= 0) continue;
    const int bx = gx + c * gw / kChannels;
    g.drawFastVLine(bx, gy + gh - hgt, hgt,
                    c == peakCh_ ? th.warn : toolsetAccent("nrf24"));
  }

  char l[40];
  std::snprintf(l, sizeof(l), "peak ch %d  %d MHz", peakCh_, 2400 + peakCh_);
  g.setTextSize(2);
  g.setTextColor(th.fg, th.bg);
  g.setCursor(8, gy + gh + 8);
  g.print(l);

  kit::end("A clear   B- back");
}

// ---------------- headless ----------------

std::string nrf24ScanJson(const std::string& params) {
  if (!rfpack::presence().nrf24) return R"({"error":"radio_absent"})";
  if (!rfpack::acquire(rfpack::Radio::Nrf24)) return R"({"error":"radio_busy"})";

  uint32_t budget = 4000;
  JsonDocument p;
  if (deserializeJson(p, params) == DeserializationError::Ok &&
      p["ms"].is<uint32_t>())
    budget = p["ms"];

  uint16_t hits[Nrf24ScanScreen::kChannels] = {0};
  radioUp();
  uint32_t sweeps = 0;
  const uint32_t start = millis();
  while (millis() - start < budget) {
    for (int c = 0; c < Nrf24ScanScreen::kChannels; ++c)
      if (carrierOn(c)) hits[c]++;
    sweeps++;
  }
  radioDown();
  rfpack::release();

  JsonDocument d;
  d["sweeps"] = sweeps;
  JsonArray ch = d["channels"].to<JsonArray>();
  int peak = 0;
  for (int c = 0; c < Nrf24ScanScreen::kChannels; ++c) {
    ch.add(hits[c]);
    if (hits[c] > hits[peak]) peak = c;
  }
  d["peak_ch"] = peak;
  d["peak_mhz"] = 2400 + peak;
  d["peak_hits"] = hits[peak];
  std::string out;
  serializeJson(d, out);
  return out;
}

#endif  // SFERIC_RF_PACK
