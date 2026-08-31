#include "toolset_label.h"

const char* toolsetLabel(const std::string& t) {
  if (t == "wifi") return "WiFi";
  if (t == "ble") return "Bluetooth";
  if (t == "ir") return "Infrared";
  if (t == "usb") return "USB";
  if (t == "sensors") return "Sensors";
  if (t == "subghz") return "SubGHz";
  if (t == "nrf24") return "2.4GHz";
  return t.c_str();
}
