#include "ir_tool.h"

#include <ArduinoJson.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

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

// ---------------- headless ----------------

namespace {
void ensureIr() {
  if (g_recv == nullptr) {
    g_recv = new IRrecv(kIrRxPin, kIrCaptureBuf, kIrTimeoutMs, true);
    g_send = new IRsend(kIrTxPin);
    g_send->begin();
  }
}
}  // namespace

std::string irCaptureLearn(const std::string& params) {
  uint32_t ms = 5000;
  JsonDocument p;
  if (deserializeJson(p, params) == DeserializationError::Ok &&
      p["ms"].is<uint32_t>())
    ms = p["ms"];
  ensureIr();
  g_recv->enableIRIn();
  g_recv->resume();

  const uint32_t start = millis();
  while (millis() - start < ms) {
    if (g_recv->decode(&g_results)) {
      JsonDocument d;
      const String proto = typeToString(g_results.decode_type, false);
      char val[24];
      std::snprintf(val, sizeof(val), "0x%llX",
                    (unsigned long long)g_results.value);
      d["protocol"] = proto;
      d["value"] = val;
      d["bits"] = g_results.bits;
      uint16_t* rb = resultToRawArray(&g_results);
      const uint16_t rlen = getCorrectedRawLength(&g_results);
      JsonArray raw = d["raw"].to<JsonArray>();
      for (uint16_t i = 0; i < rlen; ++i) raw.add(rb[i]);
      delete[] rb;
      g_recv->disableIRIn();
      std::string out;
      serializeJson(d, out);
      return out;
    }
    delay(10);
  }
  g_recv->disableIRIn();
  return R"({"error":"timeout"})";
}

std::string irReplayEmit(const std::string& params) {
  JsonDocument p;
  if (deserializeJson(p, params) != DeserializationError::Ok)
    return R"({"error":"bad_params"})";
  ensureIr();
  bool sent = false;

  if (p["raw"].is<JsonArrayConst>()) {
    std::vector<uint16_t> buf;
    for (JsonVariantConst v : p["raw"].as<JsonArrayConst>())
      buf.push_back((uint16_t)v.as<unsigned long>());
    if (!buf.empty()) {
      g_send->sendRaw(buf.data(), buf.size(), 38);
      sent = true;
    }
  } else if (!p["proto"].isNull()) {
    const long proto = p["proto"] | 0;
    const uint64_t value =
        std::strtoull(p["value"] | "0", nullptr, 0);
    const uint16_t bits = p["bits"] | 32;
    sent = g_send->send((decode_type_t)proto, value, bits);
  }
  if (g_recv) g_recv->enableIRIn();

  JsonDocument d;
  d["sent"] = sent;
  std::string out;
  serializeJson(d, out);
  return out;
}

void IrScreen::arm() {
  strike_ = Strike{};
  g_recv->disableIRIn();  // clean slate when re-arming
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
  const bool wasArmed = replayArm_.armed;
  const bool trigB = (b == BtnEvent::Click && mode_ == Mode::Captured);
  if (replayArm_.update(trigB, a == BtnEvent::Click, hal.nowMs())) {
    replay();
  } else if (a == BtnEvent::Click && !wasArmed) {
    arm();  // (re)capture
  }
  if (trigB || wasArmed != replayArm_.armed) dirty_ = true;

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
  const Theme& th = theme();
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Infrared  Learn / Replay");

  g.setTextSize(1);
  g.setCursor(6, kTitleH + 10);
  switch (mode_) {
    case Mode::Idle:
      g.setTextColor(th.fg, th.bg);
      g.print("Idle. Press A, then aim a");
      g.setCursor(6, kTitleH + 24);
      g.print("remote at the top edge.");
      break;
    case Mode::Listening:
      g.setTextColor(th.warn, th.bg);
      g.print("Listening...");
      break;
    case Mode::Captured:
      g.setTextColor(replayArm_.armed ? th.bad : th.ok, th.bg);
      g.print(replayArm_.armed ? "ARMED - B again to replay" : "Captured:");
      g.setCursor(6, kTitleH + 26);
      g.setTextColor(th.fg, th.bg);
      g.print(strike_.summary);
      g.setCursor(6, kTitleH + 42);
      g.setTextColor(th.fg, th.bg);
      g.printf("%u raw samples", (unsigned)strike_.raw.size());
      break;
  }

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print(mode_ != Mode::Captured  ? "A arm  B(hold) back"
          : replayArm_.armed       ? "B replay  A cancel  B(hold) back"
                                   : "A recapture  B arm  B(hold) back");
}
