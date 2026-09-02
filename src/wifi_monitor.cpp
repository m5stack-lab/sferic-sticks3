#include "wifi_monitor.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

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

// stream-session state (separate from the Screen's members)
uint32_t g_sRate = 0, g_sLastTotal = 0, g_sHop = 0;
uint8_t g_sCh = 1;
}  // namespace

bool wifiMonitorStreamStart(const std::string&) {
  resetCounters();
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  g_sCh = 1;
  esp_wifi_set_channel(g_sCh, WIFI_SECOND_CHAN_NONE);
  g_sRate = g_sHop = millis();
  g_sLastTotal = 0;
  return true;
}

void wifiMonitorStreamPoll(const StrikeEmit& emit) {
  const uint32_t now = millis();
  if (now - g_sHop > 250) {
    g_sHop = now;
    g_sCh = g_sCh % 13 + 1;
    esp_wifi_set_channel(g_sCh, WIFI_SECOND_CHAN_NONE);
  }
  if (now - g_sRate >= 1000) {
    const uint32_t dt = now - g_sRate;
    const uint32_t total = g_total;
    JsonDocument d;
    d["total"] = total;
    d["mgmt"] = (uint32_t)g_mgmt;
    d["data"] = (uint32_t)g_data;
    d["ctrl"] = (uint32_t)g_ctrl;
    d["rate"] = (total - g_sLastTotal) * 1000 / dt;
    d["rssi"] = g_last_rssi;
    emit("monitor", d);
    g_sLastTotal = total;
    g_sRate = now;
  }
}

void wifiMonitorStreamStop() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);
}

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
  char t[40];
  std::snprintf(t, sizeof(t), "Packet Monitor  %s%d",
                hop_ ? "hop " : "ch", ch_);
  kit::begin(t, toolsetAccent("wifi"));

  char h[20];
  std::snprintf(h, sizeof(h), "%lu/s", (unsigned long)rate_);
  kit::headline(h, theme().ok);

  char l[40];
  std::snprintf(l, sizeof(l), "total  %lu", (unsigned long)g_total);
  kit::line(0, l);
  std::snprintf(l, sizeof(l), "m%lu d%lu c%lu", (unsigned long)g_mgmt,
                (unsigned long)g_data, (unsigned long)g_ctrl);
  kit::line(1, l);
  std::snprintf(l, sizeof(l), "rssi  %d", g_last_rssi);
  kit::line(2, l, theme().fgDim);

  kit::end("A ch   A- hop   B reset   B- back");
}
