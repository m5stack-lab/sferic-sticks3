#include "usb_hid.h"

#include "hal.h"

#ifdef SFERIC_USB_HID

#include <USB.h>
#include <USBHIDKeyboard.h>

namespace {
constexpr int kTitleH = 18;
USBHIDKeyboard g_kbd;
bool g_begun = false;

const char* kPayloads[] = {
    "hello from sferic\n",
    "sferic usb hid test 1234\n",
    "the quick brown fox jumps over the lazy dog\n",
};
constexpr int kPayloadCount = sizeof(kPayloads) / sizeof(kPayloads[0]);
}  // namespace

void UsbHidScreen::onEnter() {
  dirty_ = true;
  if (!g_begun) {
    USB.begin();
    g_kbd.begin();
    g_begun = true;
  }
}

void UsbHidScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    payload_ = (payload_ + 1) % kPayloadCount;
    dirty_ = true;
  } else if (b == BtnEvent::Click) {
    Serial.printf("[usbhid] type: %s", kPayloads[payload_]);
    g_kbd.print(kPayloads[payload_]);
    dirty_ = true;
  }
  if (hal.nowMs() - lastDraw_ > 300) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = hal.nowMs();
  }
}

void UsbHidScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("USB  HID Keyboard");

  int y = kTitleH + 8;
  g.setTextColor(TFT_YELLOW, TFT_BLACK);
  g.setCursor(6, y);
  g.print("plug into a host, press B");
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, y + 18);
  g.printf("payload %d/%d:", payload_ + 1, kPayloadCount);
  g.setCursor(6, y + 32);
  g.setTextColor(TFT_WHITE, TFT_BLACK);
  std::string pv = kPayloads[payload_];
  if (!pv.empty() && pv.back() == '\n') pv.pop_back();
  g.print(pv.substr(0, 34).c_str());

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A payload  B type  B(hold) back");
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
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, 20);
  g.print("USB HID: flash the");
  g.setCursor(6, 34);
  g.print("sferic-usbhid build.");
}

#endif
