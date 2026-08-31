#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui.h"

// GATT browse — pushed from the BLE Scan decode view when a device is
// connectable. Connects, enumerates services + characteristics (with R/W/N/I
// flags and a short read of readable ones), then disconnects. A = scroll,
// B-hold = back.
class GattScreen : public Screen {
 public:
  GattScreen(std::string addr, uint8_t type) : addr_(std::move(addr)), type_(type) {}
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  enum class State : uint8_t { Start, Done, Failed };
  std::string addr_;
  uint8_t type_;
  State state_ = State::Start;
  std::vector<std::string> lines_;
  int top_ = 0;
  bool dirty_ = true;
  void connectAndBrowse();
  void draw();
};
