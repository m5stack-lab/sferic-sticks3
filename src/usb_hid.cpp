#include "usb_hid.h"

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

#ifdef SFERIC_USB_HID

#include <ArduinoJson.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

#include <cstdio>
#include <string>

namespace {
constexpr int kTitleH = 18;
USBHIDKeyboard g_kbd;
bool g_begun = false;

void usbBegin() {
  if (!g_begun) {
    USB.begin();
    g_kbd.begin();
    g_begun = true;
  }
}

const char* kPayloads[] = {
    "hello from sferic\n",
    "sferic usb hid test 1234\n",
    "the quick brown fox jumps over the lazy dog\n",
};
constexpr int kPayloadCount = sizeof(kPayloads) / sizeof(kPayloads[0]);
}  // namespace

void UsbHidScreen::onEnter() {
  dirty_ = true;
  usbBegin();
}

// ---------------- headless emit ----------------

bool usbHidArm(const std::string&) {
  usbBegin();
  return true;
}

std::string usbHidEmit(const std::string& params) {
  JsonDocument p;
  const char* text = "hello from sferic\n";
  if (deserializeJson(p, params) == DeserializationError::Ok &&
      p["text"].is<const char*>())
    text = p["text"];
  const size_t n = g_kbd.print(text);
  Serial.printf("[usbhid] headless typed %u chars\n", (unsigned)n);
  JsonDocument d;
  d["typed"] = (uint32_t)n;
  std::string out;
  serializeJson(d, out);
  return out;
}

void UsbHidScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  const bool wasArmed = typeArm_.armed;
  if (typeArm_.update(b == BtnEvent::Click, a == BtnEvent::Click,
                      hal.nowMs())) {
    Serial.printf("[usbhid] type: %s", kPayloads[payload_]);
    g_kbd.print(kPayloads[payload_]);
  } else if (a == BtnEvent::Click && !wasArmed) {
    payload_ = (payload_ + 1) % kPayloadCount;
  }
  dirty_ = true;
  if (hal.nowMs() - lastDraw_ > 300) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = hal.nowMs();
  }
}

void UsbHidScreen::draw() {
  const Theme& th = theme();
  kit::begin("HID Keyboard", toolsetAccent("ble"));

  kit::headline(typeArm_.armed ? "ARMED" : "READY",
                typeArm_.armed ? th.bad : th.warn);
  kit::line(0, typeArm_.armed ? "B again types it"
                              : "plug into a host, press B",
            th.fgDim);
  char l[40];
  std::snprintf(l, sizeof(l), "payload %d/%d", payload_ + 1, kPayloadCount);
  kit::line(1, l, th.fgDim);
  std::string pv = kPayloads[payload_];
  if (!pv.empty() && pv.back() == '\n') pv.pop_back();
  kit::marquee(pv.c_str(), 8, kit::BODY_Y + 84, kit::W - 16, 2, th.fg, 0);

  kit::end(typeArm_.armed ? "B type   A cancel   B- back"
                          : "A payload   B arm   B- back");
}

#else  // stub so the file always compiles

void UsbHidScreen::onEnter() { dirty_ = true; }
void UsbHidScreen::tick() {
  if (hal.event(Btn::B) == BtnEvent::Hold) pop();
  hal.event(Btn::A);
  if (dirty_) {
    draw();
    dirty_ = false;
  }
}
void UsbHidScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.setTextColor(th.fg, th.bg);
  g.setCursor(6, 20);
  g.print("USB HID: flash the");
  g.setCursor(6, 34);
  g.print("sferic-usbhid build.");
}

#endif
