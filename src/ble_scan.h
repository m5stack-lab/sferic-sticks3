#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui.h"

class NimBLEScan;

// Headless one-shot BLE scan (ADR-0004 §4). Blocking for `durMs`. Returns
// {"count":N,"devices":[{addr,addr_type,name,rssi,connectable,services}]}.
// BLE and the Web UI SoftAP coexist, so no radio-busy guard here.
std::string bleScanJson(uint32_t durMs = 4000);

// Bluetooth -> BLE Scan. Continuous NimBLE active scan; live list of
// advertising devices. A = next, A-hold = clear/rescan, B = open decode
// detail, B-hold = back.
class BleScanScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  NimBLEScan* scan_ = nullptr;
  bool started_ = false;
  int sel_ = 0;
  int top_ = 0;
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  uint32_t lastLog_ = 0;
  void draw();
};

// A frozen copy of one advertised device (the live scan results get freed).
struct BleSnapshot {
  std::string addr;
  uint8_t addr_type = 0;
  std::string name;
  int rssi = 0;
  uint16_t appearance = 0;
  uint8_t adv_flags = 0;
  bool connectable = false;
  std::vector<std::string> uuids;
  std::string mfr;  // raw manufacturer bytes
};

// Bluetooth -> BLE Scan detail (the "decode" view). Pushed from the scan
// list; decodes flags / services / manufacturer data / iBeacon.
class BleDetailScreen : public Screen {
 public:
  explicit BleDetailScreen(BleSnapshot s);
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  BleSnapshot s_;
  std::vector<std::string> lines_;
  int top_ = 0;
  bool dirty_ = true;
  bool built_ = false;
  void build();
  void draw();
};
