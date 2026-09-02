#include "ble_scan.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include <string>

#include "ble_common.h"
#include "ble_gatt.h"
#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kRowH = 12;
int visibleRows() { return (hal.h() - kTitleH - 10) / kRowH; }

uint16_t rssiColor(int rssi) {
  const Theme& th = theme();
  if (rssi > -60) return th.ok;
  if (rssi > -80) return th.warn;
  return th.bad;
}
}  // namespace

void BleScanScreen::onEnter() {
  dirty_ = true;
  if (started_) {
    if (scan_ && !scan_->isScanning()) scan_->start(0, false);
    return;
  }
  scan_ = bleScan();
  scan_->setActiveScan(true);
  scan_->setMaxResults(48);
  scan_->setInterval(45);
  scan_->setWindow(15);
  scan_->start(0, false);  // 0 = scan until stopped (non-blocking)
  started_ = true;
  sel_ = 0;
  top_ = 0;
}

void BleScanScreen::onExit() {
  if (!started_) return;
  bleStop();
  scan_ = nullptr;
  started_ = false;
}

void BleScanScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  const int n = scan_ ? scan_->getResults().getCount() : 0;
  if (a == BtnEvent::Click && n > 0) {
    sel_ = (sel_ + 1) % n;
    dirty_ = true;
  } else if (a == BtnEvent::Hold && n > 0) {
    sel_ = (sel_ - 1 + n) % n;
    dirty_ = true;
  } else if (b == BtnEvent::Click && scan_) {
    NimBLEScanResults r = scan_->getResults();
    if (sel_ < r.getCount()) {
      const NimBLEAdvertisedDevice* d = r.getDevice(sel_);
      BleSnapshot s;
      s.addr = d->getAddress().toString();
      s.addr_type = d->getAddressType();
      s.name = d->haveName() ? d->getName() : "";
      s.rssi = d->getRSSI();
      s.appearance = d->getAppearance();
      s.adv_flags = d->getAdvFlags();
      s.connectable = d->isConnectable();
      for (uint8_t i = 0; i < d->getServiceUUIDCount(); ++i)
        s.uuids.push_back(d->getServiceUUID(i).toString());
      if (d->haveManufacturerData() && d->getManufacturerDataCount() > 0)
        s.mfr = d->getManufacturerData(0);
      ui.push(new BleDetailScreen(std::move(s)));
      return;
    }
  }

  if (hal.nowMs() - lastDraw_ > 400) dirty_ = true;  // refresh RSSI / new devs
  if (kit::marqueeAnimating()) dirty_ = true;

  if (scan_ && hal.nowMs() - lastLog_ > 2000) {
    lastLog_ = hal.nowMs();
    Serial.printf("[ble] %d devices\n", scan_->getResults().getCount());
  }

  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = hal.nowMs();
  }
}

void BleScanScreen::draw() {
  NimBLEScanResults r = scan_ ? scan_->getResults() : NimBLEScanResults();
  const int n = r.getCount();
  const bool scanning = scan_ && scan_->isScanning();

  kit::begin(scanning ? "BLE Scan  scanning" : "BLE Scan", toolsetAccent("ble"));

  if (n == 0) {
    kit::headline("...", theme().fgDim);
    kit::end("B- back");
    return;
  }
  if (sel_ >= n) sel_ = n - 1;
  const NimBLEAdvertisedDevice* d = r.getDevice(sel_);
  if (d == nullptr) {
    kit::end("B- back");
    return;
  }

  std::string nm = d->haveName() ? d->getName() : d->getAddress().toString();
  char sub[56];
  std::snprintf(sub, sizeof(sub), "%d dBm  s%u  %s", d->getRSSI(),
                d->getServiceUUIDCount(), d->isConnectable() ? "conn" : "");
  kit::detail(nm.c_str(), sub, sel_ + 1, n);
  kit::end("< >   B decode   B- back");
}

// ---------------- headless ----------------

std::string bleScanJson(uint32_t durMs) {
  NimBLEScan* s = bleScan();
  s->setActiveScan(true);
  s->setMaxResults(48);
  s->setInterval(45);
  s->setWindow(15);
  s->clearResults();

  const NimBLEScanResults r = s->getResults(durMs, false);  // blocks durMs

  JsonDocument d;
  d["count"] = r.getCount();
  JsonArray devs = d["devices"].to<JsonArray>();
  for (int i = 0; i < r.getCount(); ++i) {
    const NimBLEAdvertisedDevice* dv = r.getDevice(i);
    if (dv == nullptr) continue;
    JsonObject o = devs.add<JsonObject>();
    o["addr"] = dv->getAddress().toString();
    o["addr_type"] = dv->getAddressType();
    o["name"] = dv->haveName() ? dv->getName() : std::string();
    o["rssi"] = dv->getRSSI();
    o["connectable"] = dv->isConnectable();
    o["services"] = dv->getServiceUUIDCount();
  }

  s->clearResults();
  bleStop();

  std::string out;
  serializeJson(d, out);
  return out;
}

// ---------------- BleDetailScreen ----------------

namespace {
std::string toHex(const std::string& b, size_t limit = 24) {
  static const char* h = "0123456789ABCDEF";
  std::string out;
  for (size_t i = 0; i < b.size() && i < limit; ++i) {
    uint8_t c = (uint8_t)b[i];
    out += h[c >> 4];
    out += h[c & 0xF];
    out += ' ';
  }
  if (b.size() > limit) out += "...";
  return out;
}
}  // namespace

BleDetailScreen::BleDetailScreen(BleSnapshot s) : s_(std::move(s)) {}

void BleDetailScreen::build() {
  lines_.clear();
  const char* at = (s_.addr_type == 0)   ? "public"
                   : (s_.addr_type == 1) ? "random"
                                         : "?";
  lines_.push_back("addr  " + s_.addr + " (" + at + ")");
  lines_.push_back("rssi  " + std::to_string(s_.rssi) + " dBm");
  lines_.push_back(std::string("name  ") + (s_.name.empty() ? "(none)" : s_.name));
  lines_.push_back(std::string("conn  ") + (s_.connectable ? "yes" : "no"));

  char buf[40];
  snprintf(buf, sizeof(buf), "flags 0x%02X   appearance 0x%04X", s_.adv_flags,
           s_.appearance);
  lines_.push_back(buf);

  lines_.push_back("services (" + std::to_string(s_.uuids.size()) + ")");
  for (const auto& u : s_.uuids) lines_.push_back("  " + u);

  if (!s_.mfr.empty()) {
    const uint8_t* m = (const uint8_t*)s_.mfr.data();
    uint16_t cid = s_.mfr.size() >= 2 ? (m[0] | (m[1] << 8)) : 0;
    snprintf(buf, sizeof(buf), "mfr (%u b)  company 0x%04X",
             (unsigned)s_.mfr.size(), cid);
    lines_.push_back(buf);
    lines_.push_back("  " + toHex(s_.mfr));

    // iBeacon: Apple (0x004C) + type 0x02 len 0x15
    if (s_.mfr.size() >= 25 && cid == 0x004C && m[2] == 0x02 && m[3] == 0x15) {
      std::string uuid;
      for (int i = 0; i < 16; ++i) {
        char x[3];
        snprintf(x, sizeof(x), "%02X", m[4 + i]);
        uuid += x;
        if (i == 3 || i == 5 || i == 7 || i == 9) uuid += '-';
      }
      uint16_t major = (m[20] << 8) | m[21];
      uint16_t minor = (m[22] << 8) | m[23];
      int8_t tx = (int8_t)m[24];
      lines_.push_back("iBeacon");
      lines_.push_back("  " + uuid);
      snprintf(buf, sizeof(buf), "  major %u  minor %u  tx %d", major, minor,
               tx);
      lines_.push_back(buf);
    }
  }
  built_ = true;
}

void BleDetailScreen::tick() {
  if (!built_) build();
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  const int rows = (hal.h() - kTitleH - 10) / 12;

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (b == BtnEvent::Click && s_.connectable) {
    ui.push(new GattScreen(s_.addr, s_.addr_type));
    return;
  }
  if (a == BtnEvent::Click) {
    top_ = (top_ + rows < (int)lines_.size()) ? top_ + 1 : 0;
    dirty_ = true;
  }
  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void BleDetailScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("BLE  decode");

  const int rows = (hal.h() - kTitleH - 10) / 12;
  g.setTextColor(th.fg, th.bg);
  for (int i = 0; i < rows && (top_ + i) < (int)lines_.size(); ++i) {
    g.setCursor(4, kTitleH + 4 + i * 12);
    g.print(lines_[top_ + i].substr(0, 40).c_str());
  }
  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print(s_.connectable ? "A scroll  B connect  B(hold) back"
                         : "A scroll  B(hold) back");
}
