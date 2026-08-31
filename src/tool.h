#pragma once

#include <functional>
#include <string>

#include "ui.h"

// A registered tool (ADR-0004). Data + a factory for its on-device view, plus
// an optional headless entry point so the web UI and (later) the MCP server
// can *launch* a tool, not just enable/disable it (ADR-0004 §4, ADR-0003 §8).
struct Tool {
  std::string id;       // "<toolset>.<name>", e.g. "wifi.scan"
  std::string name;     // menu label, e.g. "AP Scan"
  std::string toolset;  // "wifi" | "ble" | "ir" | ...
  std::string summary;  // one line
  std::function<Screen*()> make;

  // Headless run: opaque params in (query string / JSON later), result string
  // out. Null when the tool is device-menu only — e.g. it needs a radio the
  // web UI's SoftAP is already using, or it emits and must stay behind the
  // on-device arm/confirm guard. Menu, web, and MCP all dispatch through this.
  std::function<std::string(const std::string&)> run;
};

// Display label for a toolset id.
const char* toolsetLabel(const std::string& toolset);
