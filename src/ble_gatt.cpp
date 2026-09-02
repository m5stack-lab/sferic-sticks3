#include "ble_gatt.h"

#include <NimBLEDevice.h>

#include "ble_common.h"
#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

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
  const int rows = kit::listRows();
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
  kit::begin("GATT", toolsetAccent("ble"));

  if (state_ == State::Start) {
    kit::headline("LINK", th.warn);
    kit::line(0, "connecting to", th.fgDim);
    kit::line(1, addr_.c_str());
    kit::end("B- back");
    return;
  }
  if (state_ == State::Failed) {
    kit::headline("FAIL", th.bad);
    kit::line(0, "connect failed", th.fgDim);
    kit::end("B- back");
    return;
  }

  auto& g = hal.gfx();
  const int rows = kit::listRows();
  for (int i = 0; i < rows && (top_ + i) < (int)lines_.size(); ++i) {
    const std::string& ln = lines_[top_ + i];
    g.setTextSize(2);
    g.setTextColor(ln[0] == 'S' ? th.accentBle : th.fg, th.bg);
    g.setCursor(8, kit::BODY_Y + i * kit::ROW_H + 4);
    g.print(ln.substr(0, 18).c_str());
  }

  kit::end(lines_.size() > (size_t)rows ? "A scroll   B- back" : "B- back");
}
