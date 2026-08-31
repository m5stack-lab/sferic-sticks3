#include "ir_tool.h"

#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

#include "hal.h"

namespace {
constexpr uint16_t kIrRxPin = 42;
constexpr uint16_t kIrTxPin = 46;
constexpr uint16_t kIrCaptureBuf = 1024;
constexpr uint8_t kIrTimeoutMs = 15;
constexpr int kTitleH = 18;

IRrecv* g_recv = nullptr;
IRsend* g_send = nullptr;
decode_results g_results;
}  // namespace

void IrScreen::onEnter() {
  dirty_ = true;
  if (g_recv == nullptr) {
    g_recv = new IRrecv(kIrRxPin, kIrCaptureBuf, kIrTimeoutMs, true);
    g_send = new IRsend(kIrTxPin);
    g_send->begin();
  }
}

void IrScreen::onExit() {
  if (g_recv) {
    g_recv->disableIRIn();
    delete g_recv;
    g_recv = nullptr;
    delete g_send;
    g_send = nullptr;
  }
}

void IrScreen::arm() {
  strike_ = Strike{};
  g_recv->enableIRIn();
  g_recv->resume();
  mode_ = Mode::Listening;
  dirty_ = true;
  Serial.println("[ir] armed - point a remote and press a button");
}

void IrScreen::replay() {
  if (mode_ != Mode::Captured) return;
  bool sent = false;
  // Prefer a decoded resend; fall back to the raw timing buffer.
  if (strike_.proto > 0 && strike_.proto != UNKNOWN) {
    sent = g_send->send((decode_type_t)strike_.proto, strike_.value,
                        strike_.bits);
  }
  if (!sent && !strike_.raw.empty()) {
    g_send->sendRaw(strike_.raw.data(), strike_.raw.size(), 38);
    sent = true;
  }
  g_recv->enableIRIn();  // sending disables the receiver; bring it back
  Serial.printf("[ir] replayed: %s\n", sent ? strike_.summary : "nothing");
  dirty_ = true;
}

void IrScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    arm();
  } else if (b == BtnEvent::Click && mode_ == Mode::Captured) {
    replay();
  }

  if (mode_ == Mode::Listening && g_recv->decode(&g_results)) {
    strike_.proto = g_results.decode_type;
    strike_.value = g_results.value;
    strike_.bits = g_results.bits;

    uint16_t* rb = resultToRawArray(&g_results);
    const uint16_t rlen = getCorrectedRawLength(&g_results);
    strike_.raw.assign(rb, rb + rlen);
    delete[] rb;

    String s = typeToString(g_results.decode_type, false);
    if (g_results.decode_type == UNKNOWN) {
      snprintf(strike_.summary, sizeof(strike_.summary), "RAW  %u samples",
               (unsigned)rlen);
    } else {
      snprintf(strike_.summary, sizeof(strike_.summary), "%s  0x%llX / %ub",
               s.c_str(), (unsigned long long)strike_.value, strike_.bits);
    }
    g_recv->disableIRIn();
    mode_ = Mode::Captured;
    dirty_ = true;
    Serial.printf("[ir] captured: %s\n", strike_.summary);
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void IrScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Infrared  Learn / Replay");

  g.setTextSize(1);
  g.setCursor(6, kTitleH + 10);
  switch (mode_) {
    case Mode::Idle:
      g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      g.print("Idle. Press A, then aim a");
      g.setCursor(6, kTitleH + 24);
      g.print("remote at the top edge.");
      break;
    case Mode::Listening:
      g.setTextColor(TFT_YELLOW, TFT_BLACK);
      g.print("Listening...");
      break;
    case Mode::Captured:
      g.setTextColor(TFT_GREEN, TFT_BLACK);
      g.print("Captured:");
      g.setCursor(6, kTitleH + 26);
      g.setTextColor(TFT_WHITE, TFT_BLACK);
      g.print(strike_.summary);
      g.setCursor(6, kTitleH + 42);
      g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      g.printf("%u raw samples", (unsigned)strike_.raw.size());
      break;
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print(mode_ == Mode::Captured ? "A recapture  B replay  B(hold) back"
                                  : "A arm  B(hold) back");
}
