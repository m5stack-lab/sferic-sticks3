#include "ir_tvbgone.h"

#include <IRremoteESP8266.h>
#include <IRsend.h>

#include "hal.h"

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

// A small, common set. TV-B-Gone by nature is best-effort — aim, fire, hope.
const TvCode kCodes[] = {
    {NEC, 0x20DF10EF, 32, "Samsung"},   {SAMSUNG, 0xE0E040BF, 32, "Samsung2"},
    {NEC, 0x20DF10EF, 32, "LG"},        {SONY, 0xA90, 12, "Sony"},
    {SONY, 0xA90, 15, "Sony15"},        {NEC, 0x2FD48B7, 32, "Toshiba"},
    {NEC, 0x57E3E817, 32, "Vizio"},     {NEC, 0x00FFA25D, 32, "genNEC-A25D"},
    {NEC, 0x1FE48B7, 32, "TCL"},        {RC5, 0x0C, 12, "Philips"},
    {RC6, 0x0, 20, "PhilipsRC6"},       {PANASONIC, 0x40040100BCBD, 48, "Panasonic"},
    {NEC, 0x4CB340BF, 32, "Sharp/NEC"}, {NEC, 0x10EF, 16, "gen16"},
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

void TvBGoneScreen::onExit() {
  delete g_tx;
  g_tx = nullptr;
}

void TvBGoneScreen::tick() {
  const BtnEvent b = hal.event(Btn::B);
  hal.event(Btn::A);  // consume

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (b == BtnEvent::Click && state_ != State::Firing) {
    state_ = State::Firing;
    idx_ = 0;
    lastSend_ = 0;
    dirty_ = true;
  }

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
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Infrared  TV-B-Gone");

  g.setCursor(6, kTitleH + 14);
  switch (state_) {
    case State::Idle:
      g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      g.printf("%d codes ready.", kCount);
      g.setCursor(6, kTitleH + 30);
      g.print("Aim at the TV, press B.");
      break;
    case State::Firing:
      g.setTextColor(TFT_YELLOW, TFT_BLACK);
      g.printf("Sending %d/%d", idx_, kCount);
      g.setCursor(6, kTitleH + 30);
      g.setTextColor(TFT_WHITE, TFT_BLACK);
      if (idx_ > 0 && idx_ <= kCount) g.print(kCodes[idx_ - 1].name);
      break;
    case State::Done:
      g.setTextColor(TFT_GREEN, TFT_BLACK);
      g.printf("Done - %d codes sent", kCount);
      g.setCursor(6, kTitleH + 30);
      g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      g.print("B again to repeat.");
      break;
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("B fire  B(hold) back");
}
