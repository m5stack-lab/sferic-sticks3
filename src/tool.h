#pragma once

#include <functional>
#include <string>

#include "ui.h"

// A registered tool (ADR-0004). Data + a factory for its on-device view.
// A headless `run(params) -> result` field is added later (ADR-0003 §8).
struct Tool {
  std::string id;       // "<toolset>.<name>", e.g. "wifi.scan"
  std::string name;     // menu label, e.g. "AP Scan"
  std::string toolset;  // "wifi" | "ble" | "ir" | ...
  std::string summary;  // one line
  std::function<Screen*()> make;
};

// Display label for a toolset id.
const char* toolsetLabel(const std::string& toolset);
