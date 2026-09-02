#include "usb_hid.h"

#include "hal.h"
#include "theme.h"

#ifdef SFERIC_USB_HID

#include <ArduinoJson.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

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
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("USB  HID Keyboard");

  int y = kTitleH + 8;
  g.setTextColor(typeArm_.armed ? th.bad : th.warn, th.bg);
  g.setCursor(6, y);
  g.print(typeArm_.armed ? "ARMED - B again to type"
                        : "plug into a host, press B");
  g.setTextColor(th.fg, th.bg);
  g.setCursor(6, y + 18);
  g.printf("payload %d/%d:", payload_ + 1, kPayloadCount);
  g.setCursor(6, y + 32);
  g.setTextColor(th.fg, th.bg);
  std::string pv = kPayloads[payload_];
  if (!pv.empty() && pv.back() == '\n') pv.pop_back();
  g.print(pv.substr(0, 34).c_str());

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print(typeArm_.armed ? "B type  A cancel  B(hold) back"
                        : "A payload  B arm  B(hold) back");
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
