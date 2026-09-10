#include "nrf24.h"

#ifdef SFERIC_RF_PACK

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

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
  // An ESP32 reset does NOT reset the nRF24 (its own power domain), so the
  // sniff tool's config (2-byte SETUP_AW, pseudo-address, DYNPD/FEATURE) can
  // survive a reboot and skew RPD. Restore the datasheet power-on defaults
  // for everything the other tools touch, then bring RX up.
  rfpack::nrf24WriteReg(REG_CONFIG, 0x00);     // power down while reconfiguring
  rfpack::nrf24WriteReg(REG_EN_AA, 0x00);      // no auto-ack
  rfpack::nrf24WriteReg(0x02, 0x03);           // EN_RXADDR default
  rfpack::nrf24WriteReg(0x03, 0x03);           // SETUP_AW = 5-byte (default)
  rfpack::nrf24WriteReg(0x04, 0x00);           // SETUP_RETR off
  rfpack::nrf24WriteReg(REG_RF_SETUP, 0x0F);   // data rate / power (as before)
  rfpack::nrf24WriteReg(0x1C, 0x00);           // DYNPD off
  rfpack::nrf24WriteReg(0x1D, 0x00);           // FEATURE off
  rfpack::nrf24WriteReg(0x07, 0x70);           // clear IRQ latches
  rfpack::nrf24Command(0xE2);                  // FLUSH_RX
  rfpack::nrf24WriteReg(REG_CONFIG, CONFIG_RX);  // PWR_UP | PRIM_RX
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

// ---- promiscuous sniff (UNVERIFIED) --------------------------------------
// Goodspeed's trick: a 2-byte address whose bytes look like the on-air
// preamble (0x55 / 0xAA), CRC + auto-ack off, so any packet whose first bits
// match gets latched into the FIFO. Coarse and lossy — a starting point.
constexpr uint8_t REG_EN_RXADDR = 0x02;
constexpr uint8_t REG_SETUP_AW = 0x03;
constexpr uint8_t REG_RF_SETUP_R = 0x06;
constexpr uint8_t REG_STATUS = 0x07;
constexpr uint8_t REG_RX_ADDR_P0 = 0x0A;
constexpr uint8_t REG_RX_PW_P0 = 0x11;
constexpr uint8_t REG_FIFO_STATUS = 0x17;
constexpr uint8_t REG_DYNPD = 0x1C;
constexpr uint8_t REG_FEATURE = 0x1D;
constexpr uint8_t CMD_FLUSH_RX = 0xE2;
constexpr uint8_t SNIFF_PW = 32;

void sniffUp(uint8_t ch) {
  rfpack::nrf24Ce(false);
  rfpack::nrf24WriteReg(REG_CONFIG, 0x00);  // CRC off, power down first
  delay(2);
  rfpack::nrf24WriteReg(REG_EN_AA, 0x00);       // no auto-ack
  rfpack::nrf24WriteReg(REG_EN_RXADDR, 0x01);   // pipe 0 only
  rfpack::nrf24WriteReg(REG_SETUP_AW, 0x00);    // 2-byte address (illegal-but-works)
  rfpack::nrf24WriteReg(0x04, 0x00);            // SETUP_RETR off
  rfpack::nrf24WriteReg(REG_RF_CH, ch);
  rfpack::nrf24WriteReg(REG_RF_SETUP_R, 0x0E);  // 2 Mbps, 0 dBm, LNA
  const uint8_t addr[2] = {0x55, 0x55};
  rfpack::nrf24WriteRegN(REG_RX_ADDR_P0, addr, 2);
  rfpack::nrf24WriteReg(REG_RX_PW_P0, SNIFF_PW);
  rfpack::nrf24WriteReg(REG_DYNPD, 0x00);
  rfpack::nrf24WriteReg(REG_FEATURE, 0x00);
  rfpack::nrf24Command(CMD_FLUSH_RX);
  rfpack::nrf24WriteReg(REG_STATUS, 0x70);      // clear IRQ latches
  rfpack::nrf24WriteReg(REG_CONFIG, 0x73);      // mask IRQ, CRC off, PWR_UP, PRIM_RX
  delay(2);
  rfpack::nrf24Ce(true);
  delayMicroseconds(150);
}

void sniffDown() {
  rfpack::nrf24Ce(false);
  rfpack::nrf24WriteReg(REG_CONFIG, 0x00);
}

// A promiscuous no-CRC capture fills the FIFO with high-entropy garbage at
// ~240/s. The one thing that separates a real packet from noise without a
// CRC: real devices *retransmit*, so a genuine frame's leading bytes recur;
// random noise (2^40 space over 2 bytes of address + type) never collides.
// Surface a frame only once its 5-byte prefix has been seen before. First
// hit of a real frame is dropped; the retransmit(s) come through.
uint8_t g_prefixRing[48][5];
uint8_t g_prefixHead = 0;
uint32_t g_prefixCount = 0;

void sniffRingReset() {
  g_prefixHead = 0;
  g_prefixCount = 0;
  memset(g_prefixRing, 0, sizeof(g_prefixRing));
}

bool sniffCorroborated(const uint8_t* b) {
  const int have = g_prefixCount < 48 ? (int)g_prefixCount : 48;
  bool match = false;
  for (int i = 0; i < have; ++i)
    if (memcmp(g_prefixRing[i], b, 5) == 0) {
      match = true;
      break;
    }
  memcpy(g_prefixRing[g_prefixHead], b, 5);
  g_prefixHead = (g_prefixHead + 1) % 48;
  ++g_prefixCount;
  return match;
}

// Drain FIFO entries until a corroborated frame turns up (buf filled,
// returns SNIFF_PW) or the FIFO empties / a drain cap is hit (returns 0).
// Uncorroborated frames are consumed and counted in *rejected.
// Reject the degenerate frames the addressless receiver latches on Wi-Fi /
// BLE preamble energy: nearly all 0x55 / 0xAA, i.e. very few distinct bytes.
bool sniffEntropyOk(const uint8_t* b, uint8_t n) {
  bool seen[256] = {false};
  int distinct = 0;
  for (uint8_t i = 0; i < n; ++i)
    if (!seen[b[i]]) {
      seen[b[i]] = true;
      if (++distinct >= 10) return true;
    }
  return false;
}

uint8_t sniffNext(uint8_t* buf, uint32_t* rejected = nullptr) {
  for (int drained = 0; drained < 32; ++drained) {
    if (rfpack::nrf24ReadReg(REG_FIFO_STATUS) & 0x01) return 0;  // RX_EMPTY
    rfpack::nrf24ReadPayload(buf, SNIFF_PW);
    rfpack::nrf24WriteReg(REG_STATUS, 0x40);  // clear RX_DR
    // Real packet: structured (entropy) AND retransmitted (prefix recurs).
    // Random FIFO noise fails corroboration; preamble-lock frames fail
    // entropy.
    if (sniffEntropyOk(buf, SNIFF_PW) && sniffCorroborated(buf))
      return SNIFF_PW;
    if (rejected) ++*rejected;
  }
  return 0;
}

void b64(const uint8_t* in, size_t n, char* out) {
  static const char* a =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    const uint32_t v = (in[i] << 16) | ((i + 1 < n ? in[i + 1] : 0) << 8) |
                       (i + 2 < n ? in[i + 2] : 0);
    out[o++] = a[(v >> 18) & 63];
    out[o++] = a[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? a[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? a[v & 63] : '=';
  }
  out[o] = 0;
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

// ============================================================= sniff =======

void Nrf24SniffScreen::onEnter() {
  dirty_ = true;
  frames_ = 0;
  lastLen_ = 0;
  detected_ =
      rfpack::presence().nrf24 && rfpack::acquire(rfpack::Radio::Nrf24);
  if (detected_) {
    sniffRingReset();
    sniffUp(ch_);
  }
}

void Nrf24SniffScreen::onExit() {
  if (detected_) {
    sniffDown();
    rfpack::release();
  }
}

void Nrf24SniffScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click && detected_) {
    ch_ = (ch_ + 1) % 126;
    frames_ = noise_ = 0;
    sniffRingReset();
    sniffUp(ch_);
    dirty_ = true;
  }

  if (detected_) {
    uint8_t buf[32];
    for (int i = 0; i < 8; ++i) {
      const uint8_t len = sniffNext(buf, &noise_);
      if (!len) break;
      memcpy(last_, buf, len);
      lastLen_ = len;
      frames_++;
      dirty_ = true;
    }
  }

  const uint32_t now = hal.nowMs();
  if (now - lastDraw_ > 200) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void Nrf24SniffScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  char t[32];
  std::snprintf(t, sizeof(t), "Sniff  ch %d", ch_);
  kit::begin(t, toolsetAccent("nrf24"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "nRF24 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  char l[40];
  std::snprintf(l, sizeof(l), "%lu MHz", (unsigned long)(2400 + ch_));
  kit::headline(l, frames_ ? th.ok : th.fgDim);
  std::snprintf(l, sizeof(l), "sig %lu   noise %lu", (unsigned long)frames_,
                (unsigned long)noise_);
  kit::line(0, l);

  g.setTextSize(1);
  g.setTextColor(th.fgDim, th.bg);
  int y = kit::BODY_Y + kit::BODY_H - 26;
  for (int row = 0; row < 2; ++row) {
    char hex[49] = {0};
    for (int i = 0; i < 8; ++i) {
      const int idx = row * 8 + i;
      std::snprintf(hex + i * 3, 4, "%02X ", idx < lastLen_ ? last_[idx] : 0);
    }
    g.setCursor(6, y);
    g.print(hex);
    y += 11;
  }
  kit::end("A ch+   B- back");
}

bool nrf24SniffStreamStart(const std::string& params) {
  if (!rfpack::presence().nrf24) return false;
  if (!rfpack::acquire(rfpack::Radio::Nrf24)) return false;
  uint8_t ch = 2;
  JsonDocument p;
  if (deserializeJson(p, params) == DeserializationError::Ok &&
      p["ch"].is<uint8_t>())
    ch = p["ch"];
  sniffRingReset();
  sniffUp(ch);
  return true;
}

void nrf24SniffStreamPoll(const StrikeEmit& emit) {
  uint8_t buf[32];
  for (int i = 0; i < 8; ++i) {
    const uint8_t len = sniffNext(buf);
    if (!len) break;
    char enc[48];
    b64(buf, len, enc);
    JsonDocument d;
    d["bytes"] = len;
    JsonObject raw = d["raw"].to<JsonObject>();
    raw["encoding"] = "bytes-b64";
    raw["data"] = enc;
    emit("frame", d);
  }
}

void nrf24SniffStreamStop() {
  sniffDown();
  rfpack::release();
}

// =========================================================== waterfall =====

namespace {
uint8_t rpdSamples(int ch, int n) {
  uint8_t hits = 0;
  rfpack::nrf24Ce(false);
  rfpack::nrf24WriteReg(REG_RF_CH, (uint8_t)ch);
  for (int i = 0; i < n; ++i) {
    rfpack::nrf24Ce(true);
    delayMicroseconds(200);
    rfpack::nrf24Ce(false);
    if (rfpack::nrf24ReadReg(REG_RPD) & 0x01) hits++;
  }
  return hits;
}

uint16_t rpdHeat(uint8_t hits) {
  switch (hits) {
    case 0:
      return rgb565(0, 0, 24);
    case 1:
      return rgb565(0, 70, 150);
    case 2:
      return rgb565(40, 200, 120);
    default:
      return rgb565(255, 190, 0);
  }
}
}  // namespace

void Nrf24WaterfallScreen::onEnter() {
  dirty_ = true;
  head_ = 0;
  ch_ = 0;
  for (auto& r : hist_)
    for (auto& c : r) c = 0;
  for (auto& c : row_) c = 0;
  detected_ =
      rfpack::presence().nrf24 && rfpack::acquire(rfpack::Radio::Nrf24);
  if (detected_) radioUp();
}

void Nrf24WaterfallScreen::onExit() {
  if (detected_) {
    radioDown();
    rfpack::release();
  }
}

void Nrf24WaterfallScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    for (auto& r : hist_)
      for (auto& c : r) c = 0;
    dirty_ = true;
  }

  if (detected_) {
    for (int k = 0; k < 20 && ch_ < kChannels; ++k, ++ch_)
      row_[ch_] = rpdSamples(ch_, kDwell);
    if (ch_ >= kChannels) {
      ch_ = 0;
      head_ = (head_ + 1) % kRows;
      for (int i = 0; i < kChannels; ++i) hist_[head_][i] = row_[i];
      dirty_ = true;
    }
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void Nrf24WaterfallScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  kit::begin("2.4GHz Waterfall", toolsetAccent("nrf24"));

  if (!detected_) {
    kit::headline("NO RADIO", th.bad);
    kit::line(0, rfpack::present() ? "nRF24 busy" : "RF Pack not detected",
              th.fgDim);
    kit::end("B- back");
    return;
  }

  const int rh = kit::BODY_H / kRows > 0 ? kit::BODY_H / kRows : 1;
  g.startWrite();
  for (int age = 0; age < kRows; ++age) {
    const int hr = (head_ - age + kRows * 4) % kRows;
    const int y = kit::BODY_Y + kit::BODY_H - (age + 1) * rh;
    if (y < kit::BODY_Y) break;
    for (int c = 0; c < kChannels; ++c) {
      const int x0 = c * kit::W / kChannels;
      const int x1 = (c + 1) * kit::W / kChannels;
      g.fillRect(x0, y, x1 - x0 > 0 ? x1 - x0 : 1, rh, rpdHeat(hist_[hr][c]));
    }
  }
  g.endWrite();

  g.setTextSize(1);
  g.setTextColor(th.fg, th.bg);
  g.setCursor(4, kit::BODY_Y + 1);
  g.print("2400 - 2525 MHz");

  kit::end("A clear   B- back");
}

#endif  // SFERIC_RF_PACK
