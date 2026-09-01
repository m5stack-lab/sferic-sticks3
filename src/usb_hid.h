#pragma once

#include <string>

#include "confirm.h"
#include "ui.h"

// Headless emit (behind emit_arm/emit_fire; sferic-usbhid build only). arm
// brings up the USB keyboard; fire types params {"text": "..."}. Returns
// {"typed":<n>}.
bool usbHidArm(const std::string& params);
std::string usbHidEmit(const std::string& params);

// USB -> HID Keyboard. Only built under -DSFERIC_USB_HID (env sferic-usbhid,
// ADR-0005). Enumerates as a USB keyboard to the connected host; B types the
// selected payload. Authorized input testing only (ADR-0001).
class UsbHidScreen : public Screen {
 public:
  void onEnter() override;
  void tick() override;

 private:
  int payload_ = 0;
  Arm typeArm_;
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  void draw();
};
