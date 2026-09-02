#include "ir_tvbgone.h"

#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

#include <cstdio>
#include <string>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

namespace {
constexpr uint16_t kIrTxPin = 46;
constexpr int kTitleH = 18;
constexpr uint32_t kGapMs = 120;

struct TvCode {
  decode_type_t proto;
  uint64_t value;
  uint16_t bits;
  const char* name;
};

// Best-effort power/off codes for common TVs (and, by IR code-space overlap,
// a fair number of other AV gear). Aim, fire, hope. Extend freely.
const TvCode kCodes[] = {
    // Samsung
    {SAMSUNG, 0xE0E040BF, 32, "Samsung"},
    {SAMSUNG, 0xE0E019E6, 32, "Samsung2"},
    {NEC, 0x707708F7, 32, "Samsung3"},
    // LG
    {NEC, 0x20DF10EF, 32, "LG"},
    {NEC, 0x20DF23DC, 32, "LG2"},
    {NEC, 0x20DFA35C, 32, "LG3"},
    // Sony (several report lengths)
    {SONY, 0xA90, 12, "Sony12"},
    {SONY, 0xA90, 15, "Sony15"},
    {SONY, 0xA90, 20, "Sony20"},
    {SONY, 0x750, 12, "Sony12b"},
    // Panasonic
    {PANASONIC, 0x40040100BCBD, 48, "Panasonic"},
    {PANASONIC, 0x400401009C9D, 48, "Panasonic2"},
    // Toshiba
    {NEC, 0x2FD48B7, 32, "Toshiba"},
    {NEC, 0x2FDD827, 32, "Toshiba2"},
    // Sharp
    {NEC, 0x4CB340BF, 32, "Sharp"},
    {SHARP, 0x41A2, 15, "SharpP"},
    // Philips
    {RC5, 0x100C, 13, "PhilipsRC5"},
    {RC6, 0x0, 20, "PhilipsRC6"},
    // Vizio / TCL / RCA / Insignia / Hisense / Element / Sceptre / Sanyo
    {NEC, 0x57E3E817, 32, "Vizio"},
    {NEC, 0x20DF10EF, 32, "Vizio2"},
    {NEC, 0x1FE48B7, 32, "TCL"},
    {NEC, 0x1FE50AF, 32, "RCA"},
    {NEC, 0xE13DB04F, 32, "Insignia"},
    {NEC, 0x8F7A05F, 32, "Hisense"},
    {NEC, 0xFDB04F, 32, "Element"},
    {NEC, 0xC1AA09F6, 32, "Sceptre"},
    {NEC, 0x2A4CD32, 32, "Sanyo"},
    // JVC / Hitachi
    {JVC, 0x3EC1, 16, "JVC"},
    {NEC, 0x38C7A05F, 32, "Hitachi"},
    // Generic NEC power codes (cover a lot of no-name / cheap gear)
    {NEC, 0x00FFA25D, 32, "genA25D"},
    {NEC, 0x00FF00FF, 32, "gen00FF"},
    {NEC, 0x807F02FD, 32, "gen02FD"},
    {NEC, 0x00FD00FF, 32, "gen00FD"},
    {NEC, 0x00FE00FF, 32, "gen00FE"},
    {NEC, 0xE0E0E01F, 32, "genE01F"},
    {NEC, 0x10EFEB14, 32, "genEB14"},
    {NEC, 0x4CB3609F, 32, "gen609F"},
    {NEC, 0xF7C03F, 32, "genC03F"},
};
constexpr int kCount = sizeof(kCodes) / sizeof(kCodes[0]);

IRsend* g_tx = nullptr;
}  // namespace

void TvBGoneScreen::onEnter() {
  dirty_ = true;
  if (g_tx == nullptr) {
    g_tx = new IRsend(kIrTxPin);
    g_tx->begin();
  }
  state_ = State::Idle;
  idx_ = 0;
}

std::string tvBGoneEmit(const std::string&) {
  if (g_tx == nullptr) {
    g_tx = new IRsend(kIrTxPin);
    g_tx->begin();
  }
  for (int i = 0; i < kCount; ++i) {
    const TvCode& c = kCodes[i];
    g_tx->send(c.proto, c.value, c.bits);
    g_tx->send(c.proto, c.value, c.bits);
    delay(kGapMs);
  }
  Serial.printf("[tvbgone] headless sweep: %d codes\n", kCount);
  JsonDocument d;
  d["sent"] = kCount;
  std::string out;
  serializeJson(d, out);
  return out;
}

void TvBGoneScreen::onExit() {
  delete g_tx;
  g_tx = nullptr;
}

void TvBGoneScreen::tick() {
  const BtnEvent b = hal.event(Btn::B);
  hal.event(Btn::A);  // consume

  const BtnEvent a = hal.event(Btn::A);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (arm_.update(b == BtnEvent::Click && state_ != State::Firing,
                  a == BtnEvent::Click, hal.nowMs())) {
    state_ = State::Firing;
    idx_ = 0;
    lastSend_ = 0;
  }
  dirty_ = true;

  if (state_ == State::Firing && hal.nowMs() - lastSend_ >= kGapMs) {
    if (idx_ < kCount) {
      const TvCode& c = kCodes[idx_];
      g_tx->send(c.proto, c.value, c.bits);
      g_tx->send(c.proto, c.value, c.bits);  // twice, some sets need a repeat
      Serial.printf("[tvbgone] %d/%d %s\n", idx_ + 1, kCount, c.name);
      idx_++;
      lastSend_ = hal.nowMs();
      dirty_ = true;
    } else {
      state_ = State::Done;
      dirty_ = true;
      Serial.println("[tvbgone] done");
    }
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void TvBGoneScreen::draw() {
  const Theme& th = theme();
  kit::begin("TV-B-Gone", toolsetAccent("ir"));

  char l[40];
  if (arm_.armed && state_ != State::Firing) {
    kit::headline("ARMED", th.bad);
    std::snprintf(l, sizeof(l), "B fires %d codes", kCount);
    kit::line(0, l);
    kit::end("B fire   A cancel   B- back");
    return;
  }
  switch (state_) {
    case State::Idle:
      kit::headline("READY", th.fgDim);
      std::snprintf(l, sizeof(l), "%d codes loaded", kCount);
      kit::line(0, l, th.fgDim);
      kit::line(1, "aim at the TV, press B", th.fgDim);
      break;
    case State::Firing:
      std::snprintf(l, sizeof(l), "%d/%d", idx_, kCount);
      kit::headline(l, th.warn);
      if (idx_ > 0 && idx_ <= kCount) kit::line(0, kCodes[idx_ - 1].name);
      break;
    case State::Done:
      kit::headline("DONE", th.ok);
      std::snprintf(l, sizeof(l), "%d codes sent", kCount);
      kit::line(0, l, th.fgDim);
      kit::line(1, "B again to repeat", th.fgDim);
      break;
  }
  kit::end("B arm   B- back");
}
