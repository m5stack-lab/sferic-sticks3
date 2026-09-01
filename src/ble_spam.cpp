#include "ble_spam.h"

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include "ble_common.h"
#include "hal.h"

namespace {
constexpr int kTitleH = 18;
constexpr uint32_t kWindowMs = 1000;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t g_pkts = 0;
volatile uint32_t g_churn = 0;  // address changed vs. previous packet
uint64_t g_last_addr = 0;

uint64_t addr64(const NimBLEAddress& a) {
  const uint8_t* p = a.getBase()->val;
  uint64_t v = 0;
  for (int i = 0; i < 6; ++i) v = (v << 8) | p[i];
  return v;
}

class SpamCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    const uint64_t a = addr64(d->getAddress());
    portENTER_CRITICAL_ISR(&g_mux);
    g_pkts++;
    if (a != g_last_addr) {
      g_churn++;
      g_last_addr = a;
    }
    portEXIT_CRITICAL_ISR(&g_mux);
  }
};
SpamCB g_cb;

const char* verdict(uint32_t rate, uint32_t churn) {
  if (rate > 80 && churn > 40) return "SPAM";
  if (rate > 25) return "BUSY";
  return "QUIET";
}
}  // namespace

void BleSpamScreen::onEnter() {
  dirty_ = true;
  if (!started_) {
    scan_ = bleScan();
    scan_->setActiveScan(false);   // passive: we only count adverts
    scan_->setDuplicateFilter(0);  // see every packet
    scan_->setScanCallbacks(&g_cb, /*wantDuplicates=*/true);
    scan_->setInterval(30);
    scan_->setWindow(30);
    scan_->start(0, false);
    started_ = true;
  }
  g_pkts = g_churn = 0;
  winStart_ = hal.nowMs();
  peakChurn_ = 0;
}

void BleSpamScreen::onExit() {
  if (started_) {
    bleStop(&g_cb);
    scan_ = nullptr;
    started_ = false;
  }
}

void BleSpamScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  (void)a;
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (b == BtnEvent::Click) {
    peakChurn_ = 0;
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  if (now - winStart_ >= kWindowMs) {
    uint32_t pk, ch;
    portENTER_CRITICAL(&g_mux);
    pk = g_pkts;
    ch = g_churn;
    g_pkts = g_churn = 0;
    portEXIT_CRITICAL(&g_mux);
    const uint32_t dt = now - winStart_;
    rate_ = pk * 1000 / dt;
    churn_ = ch * 1000 / dt;
    if (churn_ > peakChurn_) peakChurn_ = churn_;
    winStart_ = now;
    dirty_ = true;
    Serial.printf("[blespam] %lu adv/s  %lu new-addr/s  %s\n",
                  (unsigned long)rate_, (unsigned long)churn_,
                  verdict(rate_, churn_));
  }

  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void BleSpamScreen::draw() {
  auto& g = hal.gfx();
  const char* v = verdict(rate_, churn_);
  const uint16_t vc = (v[0] == 'S') ? TFT_RED : (v[0] == 'B') ? TFT_YELLOW
                                                             : TFT_GREEN;

  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("BLE  Spam Detect");

  int y = kTitleH + 8;
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, y);
  g.printf("adv rate    %lu /s", (unsigned long)rate_);
  g.setCursor(6, y + 14);
  g.printf("new addr    %lu /s", (unsigned long)churn_);
  g.setCursor(6, y + 28);
  g.printf("peak new    %lu /s", (unsigned long)peakChurn_);

  g.setTextSize(2);
  g.setTextColor(vc, TFT_BLACK);
  g.setCursor(6, y + 46);
  g.print(v);

  g.setTextSize(1);
  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("B reset peak  B(hold) back");
}

// ---------------- headless streaming ----------------

namespace {
uint32_t g_sWin = 0;
}

bool bleSpamStreamStart(const std::string&) {
  NimBLEScan* sc = bleScan();
  sc->setActiveScan(false);
  sc->setDuplicateFilter(0);
  sc->setScanCallbacks(&g_cb, /*wantDuplicates=*/true);
  sc->setInterval(30);
  sc->setWindow(30);
  sc->start(0, false);
  portENTER_CRITICAL(&g_mux);
  g_pkts = g_churn = 0;
  portEXIT_CRITICAL(&g_mux);
  g_sWin = millis();
  return true;
}

void bleSpamStreamPoll(const StrikeEmit& emit) {
  const uint32_t now = millis();
  if (now - g_sWin < kWindowMs) return;
  uint32_t pk, ch;
  portENTER_CRITICAL(&g_mux);
  pk = g_pkts;
  ch = g_churn;
  g_pkts = g_churn = 0;
  portEXIT_CRITICAL(&g_mux);
  const uint32_t dt = now - g_sWin;
  g_sWin = now;
  const uint32_t rate = pk * 1000 / dt;
  const uint32_t churn = ch * 1000 / dt;
  JsonDocument d;
  d["rate"] = rate;
  d["churn"] = churn;
  d["verdict"] = verdict(rate, churn);
  emit("spam", d);
}

void bleSpamStreamStop() { bleStop(&g_cb); }
