#include "ble_hid.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

#include <string>

#include "ble_common.h"
#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

namespace {
constexpr int kTitleH = 18;

// Standard boot-keyboard report map (8-byte input report: mods, resv, 6 keys)
const uint8_t kReportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
    0x75, 0x08, 0x81, 0x03, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0};

const char* kPayloads[] = {
    "hello from sferic\n",
    "sferic ble hid test 1234\n",
    "the quick brown fox jumps over the lazy dog\n",
};
constexpr int kPayloadCount = sizeof(kPayloads) / sizeof(kPayloads[0]);

NimBLEServer* g_srv = nullptr;
NimBLEHIDDevice* g_hid = nullptr;
NimBLECharacteristic* g_input = nullptr;
bool g_connected = false;

class SrvCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { g_connected = true; }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    g_connected = false;
    NimBLEDevice::getAdvertising()->start();
  }
};
SrvCB g_cb;

// ascii -> {usage, shift}. Covers what the payloads use.
bool asciiToHid(char c, uint8_t& usage, bool& shift) {
  shift = false;
  if (c >= 'a' && c <= 'z') { usage = 0x04 + (c - 'a'); return true; }
  if (c >= 'A' && c <= 'Z') { usage = 0x04 + (c - 'A'); shift = true; return true; }
  if (c >= '1' && c <= '9') { usage = 0x1E + (c - '1'); return true; }
  switch (c) {
    case '0': usage = 0x27; return true;
    case ' ': usage = 0x2C; return true;
    case '\n': usage = 0x28; return true;
    case '.': usage = 0x37; return true;
    case '-': usage = 0x2D; return true;
    case '_': usage = 0x2D; shift = true; return true;
    default: return false;
  }
}

void sendReport(uint8_t mods, uint8_t key) {
  if (!g_input) return;
  uint8_t r[8] = {mods, 0, key, 0, 0, 0, 0, 0};
  g_input->setValue(r, sizeof(r));
  g_input->notify();
}

void hidSetup() {
  bleScan();  // lazy NimBLE init + stop any scan
  if (!g_srv) {
    NimBLEDevice::setDeviceName("sferic-kb");
    g_srv = NimBLEDevice::createServer();
    g_srv->setCallbacks(&g_cb);
    g_hid = new NimBLEHIDDevice(g_srv);
    g_hid->setManufacturer("sferic");
    g_hid->setPnp(0x02, 0xE502, 0xA111, 0x0210);
    g_hid->setHidInfo(0x00, 0x01);
    g_hid->setReportMap((uint8_t*)kReportMap, sizeof(kReportMap));
    g_hid->setBatteryLevel(100);
    g_input = g_hid->getInputReport(1);
    g_srv->start();
  }
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);  // HID keyboard
  adv->addServiceUUID(g_hid->getHidService()->getUUID());
  adv->enableScanResponse(true);
  adv->start();
}
}  // namespace

void BleHidScreen::onEnter() {
  dirty_ = true;
  hidSetup();
}

// ---------------- headless emit ----------------

bool bleHidArm(const std::string&) {
  hidSetup();
  return true;
}

std::string bleHidEmit(const std::string& params) {
  JsonDocument p;
  const char* text = "hello from sferic\n";
  if (deserializeJson(p, params) == DeserializationError::Ok &&
      p["text"].is<const char*>())
    text = p["text"];

  if (!g_connected) return R"({"error":"not_paired"})";

  int n = 0;
  for (const char* s = text; *s; ++s) {
    uint8_t u;
    bool sh;
    if (!asciiToHid(*s, u, sh)) continue;
    sendReport(sh ? 0x02 : 0x00, u);
    delay(6);
    sendReport(0, 0);
    delay(6);
    ++n;
  }
  sendReport(0, 0);

  Serial.printf("[hid] headless typed %d chars\n", n);
  JsonDocument d;
  d["typed"] = n;
  std::string out;
  serializeJson(d, out);
  return out;
}

void BleHidScreen::onExit() {
  NimBLEDevice::getAdvertising()->stop();
  if (g_connected && g_srv) {
    for (auto& info : g_srv->getPeerDevices()) g_srv->disconnect(info);
  }
}

void BleHidScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  const bool wasArmed = typeArm_.armed;
  if (!sending_ &&
      typeArm_.update(b == BtnEvent::Click && g_connected,
                      a == BtnEvent::Click, hal.nowMs())) {
    sending_ = true;
    sendPos_ = 0;
    lastKey_ = 0;
    Serial.printf("[hid] typing: %s", kPayloads[payload_]);
  } else if (!sending_ && a == BtnEvent::Click && !wasArmed) {
    payload_ = (payload_ + 1) % kPayloadCount;
  }
  dirty_ = true;

  if (sending_ && hal.nowMs() - lastKey_ > 12) {
    const char* s = kPayloads[payload_];
    if (s[sendPos_] == '\0') {
      sendReport(0, 0);  // release
      sending_ = false;
      dirty_ = true;
    } else {
      uint8_t u;
      bool sh;
      if (asciiToHid(s[sendPos_], u, sh)) {
        sendReport(sh ? 0x02 : 0x00, u);
        sendReport(0, 0);  // key up
      }
      sendPos_++;
      lastKey_ = hal.nowMs();
    }
  }

  if (hal.nowMs() - lastDraw_ > 300) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = hal.nowMs();
  }
}

void BleHidScreen::draw() {
  const Theme& th = theme();
  kit::begin("HID Keyboard", toolsetAccent("ble"));

  kit::headline(typeArm_.armed ? "ARMED"
                : sending_      ? "TYPING"
                : g_connected   ? "PAIRED"
                                : "ADVERT",
                typeArm_.armed ? th.bad : g_connected ? th.ok : th.warn);

  char l[40];
  std::snprintf(l, sizeof(l), "payload %d/%d", payload_ + 1, kPayloadCount);
  kit::line(0, l, th.fgDim);
  std::string pv = kPayloads[payload_];
  if (!pv.empty() && pv.back() == '\n') pv.pop_back();
  kit::marquee(pv.c_str(), 8, kit::BODY_Y + 62, kit::W - 16, 2, th.fg, 0);

  kit::end(typeArm_.armed ? "B type   A cancel"
           : g_connected  ? "A payload   B arm   B- back"
                          : "A payload   B- back");
}
