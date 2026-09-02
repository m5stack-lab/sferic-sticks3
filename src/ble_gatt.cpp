#include "ble_gatt.h"

#include <NimBLEDevice.h>

#include "ble_common.h"
#include "hal.h"
#include "theme.h"

namespace {
constexpr int kTitleH = 18;

std::string readable(const NimBLEAttValue& v) {
  std::string out;
  bool printable = v.length() > 0;
  for (size_t i = 0; i < v.length() && i < 20; ++i) {
    uint8_t c = v[i];
    if (c < 0x20 || c > 0x7E) printable = false;
  }
  if (printable) {
    out.assign((const char*)v.data(),
               v.length() < 20 ? v.length() : 20);
    return "\"" + out + "\"";
  }
  static const char* h = "0123456789ABCDEF";
  for (size_t i = 0; i < v.length() && i < 12; ++i) {
    out += h[v[i] >> 4];
    out += h[v[i] & 0xF];
  }
  if (v.length() > 12) out += "..";
  return out;
}
}  // namespace

void GattScreen::connectAndBrowse() {
  NimBLEScan* s = bleScan();
  if (s->isScanning()) s->stop();  // free the radio for a connection

  NimBLEClient* c = NimBLEDevice::createClient();
  c->setConnectTimeout(4000);
  NimBLEAddress a(addr_, type_);
  if (!c->connect(a)) {
    NimBLEDevice::deleteClient(c);
    state_ = State::Failed;
    Serial.printf("[gatt] connect failed %s\n", addr_.c_str());
    return;
  }
  Serial.printf("[gatt] connected %s\n", addr_.c_str());

  const auto& svcs = c->getServices(true);
  for (auto* svc : svcs) {
    lines_.push_back("SVC " + svc->getUUID().toString());
    const auto& chars = svc->getCharacteristics(true);
    for (auto* ch : chars) {
      std::string p;
      if (ch->canRead()) p += 'R';
      if (ch->canWrite()) p += 'W';
      if (ch->canNotify()) p += 'N';
      if (ch->canIndicate()) p += 'I';
      std::string line = " " + ch->getUUID().toString() + " [" + p + "]";
      if (ch->canRead()) line += " " + readable(ch->readValue());
      lines_.push_back(line);
    }
  }
  c->disconnect();
  NimBLEDevice::deleteClient(c);
  state_ = State::Done;
  Serial.printf("[gatt] %u services, %u lines\n", (unsigned)svcs.size(),
                (unsigned)lines_.size());
}

void GattScreen::tick() {
  if (state_ == State::Start) {
    draw();  // paint "connecting..." first
    connectAndBrowse();
    dirty_ = true;
    return;
  }

  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold || b == BtnEvent::Click) {
    pop();
    return;
  }
  const int rows = (hal.h() - kTitleH - 10) / 11;
  if (a == BtnEvent::Click) {
    top_ = (top_ + rows < (int)lines_.size()) ? top_ + 1 : 0;
    dirty_ = true;
  }
  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void GattScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("BLE  GATT");

  g.setTextSize(1);
  if (state_ == State::Start) {
    g.setTextColor(th.warn, th.bg);
    g.setCursor(6, kTitleH + 14);
    g.printf("Connecting to");
    g.setCursor(6, kTitleH + 28);
    g.print(addr_.c_str());
  } else if (state_ == State::Failed) {
    g.setTextColor(th.bad, th.bg);
    g.setCursor(6, kTitleH + 14);
    g.print("Connect failed.");
  } else {
    const int rows = (hal.h() - kTitleH - 10) / 11;
    for (int i = 0; i < rows && (top_ + i) < (int)lines_.size(); ++i) {
      const std::string& ln = lines_[top_ + i];
      g.setTextColor(ln[0] == 'S' ? th.accentBle : th.fg, th.bg);
      g.setCursor(2, kTitleH + 3 + i * 11);
      g.print(ln.substr(0, 40).c_str());
    }
  }

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print(state_ == State::Done ? "A scroll  B back" : "B back");
}
