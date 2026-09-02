#include "ir_decode.h"

#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>

#include <cstdio>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

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
  const Theme& th = theme();
  const bool recent = hal.nowMs() - lastHit_ < 500;
  kit::begin(recent ? "Protocol Decode  <<" : "Protocol Decode",
             toolsetAccent("ir"));

  if (hist_.empty()) {
    kit::headline("...", th.fgDim);
    kit::line(0, "aim a remote at the top edge", th.fgDim);
    kit::end("B- back");
    return;
  }
  // headline = latest, marqueed; then the two prior hits
  const int top = kit::TITLE_H;
  hal.gfx().fillRect(0, top, kit::W, kit::H - 12 - top, th.bg);
  kit::marquee(hist_[0].c_str(), 8, top + 10, kit::W - 16, 3,
               recent ? th.ok : th.fg, 0);
  for (size_t i = 1; i < hist_.size() && i < 3; ++i)
    kit::line((int)i - 1, hist_[i].c_str(), th.fgDim);
  kit::end("B- back");
}

// ---------------- headless ----------------

std::string irDecodeJson(uint32_t timeoutMs) {
  if (!g_rx) g_rx = new IRrecv(kRxPin, 1024, 15, true);
  g_rx->enableIRIn();

  decode_results res;
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (g_rx->decode(&res)) {
      JsonDocument d;
      const String proto = typeToString(res.decode_type, false);
      if (res.decode_type == UNKNOWN) {
        d["protocol"] = "RAW";
        d["samples"] = res.rawlen;
      } else {
        char val[24];
        std::snprintf(val, sizeof(val), "0x%llX",
                      (unsigned long long)res.value);
        d["protocol"] = proto;
        d["value"] = val;
        d["bits"] = res.bits;
      }
      g_rx->resume();
      g_rx->disableIRIn();
      std::string out;
      serializeJson(d, out);
      return out;
    }
    delay(10);
  }
  g_rx->disableIRIn();
  return R"({"error":"timeout"})";
}
