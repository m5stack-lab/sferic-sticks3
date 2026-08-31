#pragma once

#include "ui.h"

// Settings > Capture — inspect / clear / export the shared pcap buffer.
// A = clear, B = dump base64 to serial, B-hold = back. (Future: hand off to
// the MCP server or a peer device instead of serial.)
class CaptureScreen : public Screen {
 public:
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  void draw();
};
