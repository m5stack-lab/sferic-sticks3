#pragma once

#include "ui.h"

// USB -> HID Keyboard. Only built under -DSFERIC_USB_HID (env sferic-usbhid,
// ADR-0005). Enumerates as a USB keyboard to the connected host; B types the
// selected payload. Authorized input testing only (ADR-0001).
class UsbHidScreen : public Screen {
 public:
  void onEnter() override;
  void tick() override;

 private:
  int payload_ = 0;
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  void draw();
};
