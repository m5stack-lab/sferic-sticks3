#include "wifi_probe.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include <algorithm>
#include <cstdio>

#include "hal.h"
#include "theme.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kMaxEntries = 40;

// SPSC ring: WiFi task is the sole producer, tick() the sole consumer.
struct RawProbe {
  uint8_t mac[6];
  char ssid[33];
  int8_t rssi;
};
constexpr uint8_t kRing = 32;
volatile RawProbe g_ring[kRing];
volatile uint8_t g_head = 0, g_tail = 0;

void sniff(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  auto* p = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint8_t* f = p->payload;
  const uint16_t len = p->rx_ctrl.sig_len;
  if (len < 26) return;
  const uint8_t fc0 = f[0];
  if (((fc0 >> 2) & 0x3) != 0 || ((fc0 >> 4) & 0xF) != 4) return;  // mgmt/probe-req

  const uint8_t tag = f[24];
  uint8_t slen = f[25];
  if (tag != 0 || 26 + slen > len) return;
  if (slen > 32) slen = 32;

  const uint8_t h = g_head;
  const uint8_t nh = (h + 1) & (kRing - 1);
  if (nh == g_tail) return;  // full, drop
  memcpy((void*)g_ring[h].mac, f + 10, 6);
  memcpy((void*)g_ring[h].ssid, f + 26, slen);
  g_ring[h].ssid[slen] = '\0';
  g_ring[h].rssi = p->rx_ctrl.rssi;
  g_head = nh;
}
}  // namespace

void WifiProbeScreen::onEnter() {
  dirty_ = true;
  entries_.clear();
  total_ = 0;
  g_head = g_tail = 0;
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
}

void WifiProbeScreen::onExit() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);
}

void WifiProbeScreen::merge(const uint8_t* mac, const char* ssid, int8_t rssi,
                            uint32_t now) {
  for (auto& e : entries_) {
    if (memcmp(e.mac, mac, 6) == 0 && e.ssid == ssid) {
      e.rssi = rssi;
      e.count++;
      e.last = now;
      return;
    }
  }
  if (entries_.size() >= kMaxEntries) {
    size_t oldest = 0;
    for (size_t i = 1; i < entries_.size(); ++i)
      if (entries_[i].last < entries_[oldest].last) oldest = i;
    entries_.erase(entries_.begin() + oldest);
  }
  Entry e{};
  memcpy(e.mac, mac, 6);
  e.ssid = ssid;
  e.rssi = rssi;
  e.count = 1;
  e.last = now;
  entries_.push_back(std::move(e));
}

void WifiProbeScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Hold) {
    entries_.clear();
    total_ = 0;
    top_ = 0;
    dirty_ = true;
  } else if (a == BtnEvent::Click) {
    top_++;
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  bool got = false;
  while (g_tail != g_head) {
    RawProbe r;
    memcpy(&r, (const void*)&g_ring[g_tail], sizeof(r));
    g_tail = (g_tail + 1) & (kRing - 1);
    merge(r.mac, r.ssid, r.rssi, now);
    total_++;
    got = true;
  }
  if (got) dirty_ = true;

  if (now - lastHop_ > 300) {
    lastHop_ = now;
    ch_ = ch_ % 11 + 1;
    esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
  }
  if (now - lastDraw_ > 500) dirty_ = true;

  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void WifiProbeScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.printf("Probes  %u uniq / %u  ch%d", (unsigned)entries_.size(),
           (unsigned)total_, ch_);

  // most recent first
  std::vector<const Entry*> v;
  for (const auto& e : entries_) v.push_back(&e);
  std::sort(v.begin(), v.end(),
            [](const Entry* x, const Entry* y) { return x->last > y->last; });

  const int rows = (hal.h() - kTitleH - 10) / 11;
  if (top_ > (int)v.size() - rows) top_ = 0;
  if (top_ < 0) top_ = 0;

  g.setTextSize(1);
  for (int i = 0; i < rows && (top_ + i) < (int)v.size(); ++i) {
    const Entry* e = v[top_ + i];
    g.setTextColor(th.fg, th.bg);
    g.setCursor(2, kTitleH + 3 + i * 11);
    g.printf("%02X%02X%02X%02X%02X%02X %-8s %d", e->mac[0], e->mac[1], e->mac[2],
             e->mac[3], e->mac[4], e->mac[5],
             e->ssid.empty() ? "<any>" : e->ssid.substr(0, 8).c_str(),
             e->rssi);
  }

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print("A scroll  A(hold) clear  B(hold) back");
}

// ---------------- headless ----------------

std::string wifiProbeJson(uint32_t windowMs) {
  if (WiFi.getMode() & WIFI_MODE_AP)
    return R"({"error":"radio_busy"})";

  struct Acc {
    uint8_t mac[6];
    String ssid;
    int8_t rssi;
    uint32_t count;
  };
  std::vector<Acc> acc;
  uint32_t total = 0;

  g_head = g_tail = 0;
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);

  const uint32_t start = millis();
  int ch = 1;
  while (millis() - start < windowMs) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    ch = ch % 11 + 1;
    delay(120);
    while (g_tail != g_head) {
      RawProbe r;
      memcpy(&r, (const void*)&g_ring[g_tail], sizeof(r));
      g_tail = (g_tail + 1) & (kRing - 1);
      ++total;
      bool merged = false;
      for (auto& e : acc) {
        if (memcmp(e.mac, r.mac, 6) == 0 && e.ssid == r.ssid) {
          e.rssi = r.rssi;
          ++e.count;
          merged = true;
          break;
        }
      }
      if (!merged && acc.size() < 64) {
        Acc e{};
        memcpy(e.mac, r.mac, 6);
        e.ssid = r.ssid;
        e.rssi = r.rssi;
        e.count = 1;
        acc.push_back(std::move(e));
      }
    }
  }

  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);

  JsonDocument d;
  d["total"] = total;
  d["uniq"] = (uint32_t)acc.size();
  JsonArray arr = d["probes"].to<JsonArray>();
  for (const auto& e : acc) {
    JsonObject o = arr.add<JsonObject>();
    char mac[18];
    std::snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", e.mac[0],
                  e.mac[1], e.mac[2], e.mac[3], e.mac[4], e.mac[5]);
    o["mac"] = mac;
    o["ssid"] = e.ssid;
    o["rssi"] = e.rssi;
    o["count"] = e.count;
  }

  std::string out;
  serializeJson(d, out);
  return out;
}
