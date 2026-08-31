#pragma once

#include <string>

// Display label for a toolset id ("wifi" -> "WiFi"). Kept dependency-free
// (no M5 / Arduino) so it is reachable from the host test env.
const char* toolsetLabel(const std::string& toolset);
