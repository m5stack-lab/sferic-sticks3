#pragma once

#include <cstdint>

// Palette. Fields are *roles*, not hues — swapping the active Theme is the
// whole theme change. Colors are RGB565. Solarized (Ethan Schoonover);
// two variants ship, selectable in Settings > Display and persisted to NVS.

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace sol {
constexpr uint16_t base03 = rgb565(0x00, 0x2b, 0x36);
constexpr uint16_t base02 = rgb565(0x07, 0x36, 0x42);
constexpr uint16_t base01 = rgb565(0x58, 0x6e, 0x75);
constexpr uint16_t base00 = rgb565(0x65, 0x7b, 0x83);
constexpr uint16_t base0 = rgb565(0x83, 0x94, 0x96);
constexpr uint16_t base1 = rgb565(0x93, 0xa1, 0xa1);
constexpr uint16_t base2 = rgb565(0xee, 0xe8, 0xd5);
constexpr uint16_t base3 = rgb565(0xfd, 0xf6, 0xe3);
constexpr uint16_t yellow = rgb565(0xb5, 0x89, 0x00);
constexpr uint16_t orange = rgb565(0xcb, 0x4b, 0x16);
constexpr uint16_t red = rgb565(0xdc, 0x32, 0x2f);
constexpr uint16_t magenta = rgb565(0xd3, 0x36, 0x82);
constexpr uint16_t violet = rgb565(0x6c, 0x71, 0xc4);
constexpr uint16_t blue = rgb565(0x26, 0x8b, 0xd2);
constexpr uint16_t cyan = rgb565(0x2a, 0xa1, 0x98);
constexpr uint16_t green = rgb565(0x85, 0x99, 0x00);
}  // namespace sol

struct Theme {
  const char* name;
  uint16_t bg;      // screen background
  uint16_t bgAlt;   // title bar, raised rows
  uint16_t fg;      // primary text
  uint16_t fgDim;   // secondary / hint text
  uint16_t selFg;   // text on the selected row
  uint16_t selBg;   // selected-row fill (neutral — accents stay for status/icons)
  uint16_t line;    // hairlines
  uint16_t ok;      // enabled / quiet / good
  uint16_t warn;    // caution
  uint16_t bad;     // alert / attack
  uint16_t accentWifi;
  uint16_t accentBle;
  uint16_t accentIr;
  uint16_t accentSensors;
  uint16_t accentSubghz;
  uint16_t accentNrf24;
};

// Order is stable (NVS stores the index). Solarized Dark is the default
// (kDefaultTheme); Light is the alt.
inline constexpr Theme kThemes[] = {
    {"Light", sol::base3, sol::base2, sol::base00, sol::base1, sol::base3,
     sol::base1, sol::base2, sol::green, sol::yellow, sol::red, sol::blue,
     sol::cyan, sol::orange, sol::green, sol::violet, sol::magenta},
    {"Dark", sol::base03, sol::base02, sol::base0, sol::base01, sol::base3,
     sol::base01, sol::base02, sol::green, sol::yellow, sol::red, sol::blue,
     sol::cyan, sol::orange, sol::green, sol::violet, sol::magenta},
};
constexpr uint8_t kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);
constexpr uint8_t kDefaultTheme = 1;  // Solarized Dark

const Theme& theme();              // the active palette
uint8_t themeIndex();
void setThemeIndex(uint8_t i);     // wraps, persists to NVS
void loadThemePref();              // call once at boot, before the first draw

// Toolset id ("wifi", "ble", …) -> its accent in the active theme.
uint16_t toolsetAccent(const char* toolsetId);
