#include "wifi_monitor.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include "hal.h"

namespace {
constexpr int kTitleH = 18;

volatile uint32_t g_total = 0, g_mgmt = 0, g_data = 0, g_ctrl = 0;
volatile int8_t g_last_rssi = 0;

void sniff(void* buf, wifi_promiscuous_pkt_type_t type) {
  auto* p = static_cast<wifi_promiscuous_pkt_t*>(buf);
  g_total++;
  switch (type) {
    case WIFI_PKT_MGMT: g_mgmt++; break;
    case WIFI_PKT_DATA: g_data++; break;
    case WIFI_PKT_CTRL: g_ctrl++; break;
    default: break;
  }
  g_last_rssi = p->rx_ctrl.rssi;
}

void resetCounters() { g_total = g_mgmt = g_data = g_ctrl = 0; }
}  // namespace

void WifiMonitorScreen::onEnter() {
  dirty_ = true;
  resetCounters();
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
  lastRate_ = hal.nowMs();
  lastTotal_ = 0;
}

void WifiMonitorScreen::onExit() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);
}

void WifiMonitorScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    ch_ = ch_ % 13 + 1;
    hop_ = false;
    esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
    dirty_ = true;
  } else if (a == BtnEvent::Hold) {
    hop_ = !hop_;
    dirty_ = true;
  } else if (b == BtnEvent::Click) {
    resetCounters();
    lastTotal_ = 0;
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  if (hop_ && now - lastHop_ > 250) {
    lastHop_ = now;
    ch_ = ch_ % 13 + 1;
    esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
  }
  if (now - lastRate_ >= 1000) {
    rate_ = (g_total - lastTotal_) * 1000 / (now - lastRate_);
    lastTotal_ = g_total;
    lastRate_ = now;
    dirty_ = true;
    Serial.printf("[wifimon] ch=%d%s total=%lu m=%lu d=%lu c=%lu %lu/s rssi=%d\n",
                  ch_, hop_ ? "(hop)" : "", (unsigned long)g_total,
                  (unsigned long)g_mgmt, (unsigned long)g_data,
                  (unsigned long)g_ctrl, (unsigned long)rate_, g_last_rssi);
  }
  if (now - lastDraw_ > 300) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void WifiMonitorScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  if (hop_)
    g.printf("WiFi Monitor  ch hop (%d)", ch_);
  else
    g.printf("WiFi Monitor  ch %d", ch_);

  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  int y = kTitleH + 8;
  g.setCursor(6, y);
  g.printf("total  %lu", (unsigned long)g_total);
  g.setCursor(6, y + 14);
  g.printf("mgmt %lu  data %lu  ctrl %lu", (unsigned long)g_mgmt,
           (unsigned long)g_data, (unsigned long)g_ctrl);
  g.setCursor(6, y + 28);
  g.setTextColor(TFT_GREEN, TFT_BLACK);
  g.printf("%lu frames/s", (unsigned long)rate_);
  g.setCursor(6, y + 42);
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.printf("last rssi  %d", g_last_rssi);

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A ch  A(hold) hop  B reset  B(hold) back");
}
