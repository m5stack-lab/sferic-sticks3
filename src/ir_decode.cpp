#include "ir_decode.h"

#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>

#include "hal.h"

namespace {
constexpr uint16_t kRxPin = 42;
constexpr int kTitleH = 18;
constexpr size_t kHist = 6;

IRrecv* g_rx = nullptr;
decode_results g_res;
}  // namespace

void IrDecodeScreen::onEnter() {
  dirty_ = true;
  if (!g_rx) g_rx = new IRrecv(kRxPin, 1024, 15, true);
  g_rx->enableIRIn();
}

void IrDecodeScreen::onExit() {
  if (g_rx) g_rx->disableIRIn();
}

void IrDecodeScreen::tick() {
  const BtnEvent b = hal.event(Btn::B);
  hal.event(Btn::A);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }

  if (g_rx && g_rx->decode(&g_res)) {
    char line[48];
    String proto = typeToString(g_res.decode_type, false);
    if (g_res.decode_type == UNKNOWN) {
      snprintf(line, sizeof(line), "RAW  %u samp", (unsigned)g_res.rawlen);
    } else {
      snprintf(line, sizeof(line), "%-9s 0x%llX/%ub", proto.c_str(),
               (unsigned long long)g_res.value, g_res.bits);
    }
    hist_.insert(hist_.begin(), line);
    if (hist_.size() > kHist) hist_.pop_back();
    lastHit_ = hal.nowMs();
    dirty_ = true;
    Serial.printf("[irdecode] %s\n", line);
    g_rx->resume();
  }

  if (hal.nowMs() - lastHit_ < 1200 && !dirty_) dirty_ = true;  // blink "hit"
  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void IrDecodeScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  const bool recent = hal.nowMs() - lastHit_ < 400;
  g.print(recent ? "IR Decode  <<" : "IR Decode  listening");

  for (size_t i = 0; i < hist_.size(); ++i) {
    g.setTextColor(i == 0 ? TFT_GREEN : TFT_LIGHTGREY, TFT_BLACK);
    g.setCursor(4, kTitleH + 6 + (int)i * 13);
    g.print(hist_[i].c_str());
  }
  if (hist_.empty()) {
    g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    g.setCursor(6, kTitleH + 16);
    g.print("Aim a remote at the top edge.");
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("B(hold) back");
}
