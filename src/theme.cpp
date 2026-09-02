#include "theme.h"

#include <Preferences.h>

#include <cstring>

namespace {
uint8_t g_idx = kDefaultTheme;
constexpr const char* kNs = "sferic";
constexpr const char* kKey = "disp.theme";
}  // namespace

const Theme& theme() { return kThemes[g_idx]; }
uint8_t themeIndex() { return g_idx; }

void setThemeIndex(uint8_t i) {
  g_idx = i % kThemeCount;
  Preferences p;
  if (p.begin(kNs, /*readOnly=*/false)) {
    p.putUChar(kKey, g_idx);
    p.end();
  }
}

void loadThemePref() {
  Preferences p;
  if (p.begin(kNs, /*readOnly=*/true)) {
    g_idx = p.getUChar(kKey, kDefaultTheme);
    p.end();
  }
  if (g_idx >= kThemeCount) g_idx = 0;
}

uint16_t toolsetAccent(const char* t) {
  const Theme& th = theme();
  if (!t) return th.fg;
  if (!std::strcmp(t, "wifi")) return th.accentWifi;
  if (!std::strcmp(t, "ble")) return th.accentBle;
  if (!std::strcmp(t, "ir")) return th.accentIr;
  if (!std::strcmp(t, "sensors")) return th.accentSensors;
  if (!std::strcmp(t, "subghz")) return th.accentSubghz;
  if (!std::strcmp(t, "nrf24")) return th.accentNrf24;
  return th.fg;
}
