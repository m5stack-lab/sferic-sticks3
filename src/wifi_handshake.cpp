#include "wifi_handshake.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <string.h>

#include <cstdio>
#include <vector>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"
#include "pcap.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kRowH = 12;
constexpr size_t kBufBytes = 512 * 1024;

// SPSC ring of "interesting" frame headers to merge into the AP list.
struct Hit {
  uint8_t bssid[6];
  char ssid[33];  // "" when not a beacon
  bool eapol;
  int8_t rssi;
};
constexpr uint8_t kRing = 32;
volatile Hit g_ring[kRing];
volatile uint8_t g_head = 0, g_tail = 0;

void push(const Hit& h) {
  const uint8_t nh = (g_head + 1) & (kRing - 1);
  if (nh == g_tail) return;
  memcpy((void*)&g_ring[g_head], &h, sizeof(h));
  g_head = nh;
}

uint8_t g_channel = 1;

void sniff(void* buf, wifi_promiscuous_pkt_type_t type) {
  auto* p = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint8_t* f = p->payload;
  const uint16_t len = p->rx_ctrl.sig_len;
  if (len < 24) return;

  const uint8_t fc0 = f[0];
  const uint8_t ftype = (fc0 >> 2) & 0x3;
  const uint8_t subtype = (fc0 >> 4) & 0xF;

  if (ftype == 0 && subtype == 8) {  // beacon
    Hit h{};
    memcpy(h.bssid, f + 16, 6);  // addr3 = BSSID
    h.rssi = p->rx_ctrl.rssi;
    // tagged params start at 36 (24 hdr + 12 fixed); tag 0 = SSID
    if (len > 38 && f[36] == 0) {
      uint8_t sl = f[37];
      if (sl > 32) sl = 32;
      if (38 + sl <= len) {
        memcpy(h.ssid, f + 38, sl);
        h.ssid[sl] = '\0';
      }
    }
    h.eapol = false;
    push(h);
    capture.writeFrame(f, len, p->rx_ctrl.rssi, g_channel, esp_timer_get_time());
    return;
  }

  if (ftype == 2) {  // data
    const bool qos = subtype & 0x08;
    const uint16_t hdr = 24 + (qos ? 2 : 0);
    if (len < hdr + 8) return;
    static const uint8_t snap_eapol[8] = {0xAA, 0xAA, 0x03, 0x00,
                                          0x00, 0x00, 0x88, 0x8E};
    if (memcmp(f + hdr, snap_eapol, 8) != 0) return;
    Hit h{};
    memcpy(h.bssid, f + 16, 6);  // addr3 = BSSID for infra
    h.ssid[0] = '\0';
    h.eapol = true;
    h.rssi = p->rx_ctrl.rssi;
    push(h);
    capture.writeFrame(f, len, p->rx_ctrl.rssi, g_channel, esp_timer_get_time());
  }
}
}  // namespace

void WifiHandshakeScreen::onEnter() {
  dirty_ = true;
  aps_.clear();
  top_ = 0;
  capture.begin(kBufBytes);
  capture.reset();
  capture.setRecording(true);
  g_head = g_tail = 0;
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
  g_channel = ch_;
}

void WifiHandshakeScreen::onExit() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  capture.setRecording(false);
  WiFi.mode(WIFI_OFF);
}

void WifiHandshakeScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (b == BtnEvent::Click) {
    Serial.printf("[handshake] export %u frames %u bytes\n", capture.frames(),
                  (unsigned)capture.used());
    capture.dumpBase64(Serial);
  } else if (a == BtnEvent::Click && !aps_.empty()) {
    top_ = (top_ + 1) % (int)aps_.size();
    dirty_ = true;
  } else if (a == BtnEvent::Hold && !aps_.empty()) {
    top_ = (top_ - 1 + (int)aps_.size()) % (int)aps_.size();
    dirty_ = true;
  }
  if (kit::marqueeAnimating()) dirty_ = true;

  bool got = false;
  while (g_tail != g_head) {
    Hit h;
    memcpy(&h, (const void*)&g_ring[g_tail], sizeof(h));
    g_tail = (g_tail + 1) & (kRing - 1);
    got = true;

    Ap* ap = nullptr;
    for (auto& x : aps_)
      if (memcmp(x.bssid, h.bssid, 6) == 0) {
        ap = &x;
        break;
      }
    if (!ap) {
      if (aps_.size() >= 24) continue;
      aps_.push_back(Ap{});
      ap = &aps_.back();
      memcpy(ap->bssid, h.bssid, 6);
      ap->eapol = 0;
      ap->ssid = "?";
    }
    ap->rssi = h.rssi;
    if (h.ssid[0]) ap->ssid = h.ssid;
    if (h.eapol && ap->eapol < 255) ap->eapol++;
  }
  if (got) dirty_ = true;

  const uint32_t now = hal.nowMs();
  if (now - lastHop_ > 400) {
    lastHop_ = now;
    ch_ = ch_ % 11 + 1;
    g_channel = ch_;
    esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
  }
  if (now - lastDraw_ > 500) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

// ---------------- headless streaming ----------------

namespace {
struct SHAp {
  uint8_t bssid[6];
  char ssid[33];
  uint8_t eapol;
  bool announced;
};
std::vector<SHAp> g_shAps;
uint32_t g_shHop = 0;

void bssidStr(const uint8_t* b, char out[18]) {
  std::snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", b[0], b[1], b[2], b[3],
                b[4], b[5]);
}
}  // namespace

bool wifiHandshakeStreamStart(const std::string&) {
  g_shAps.clear();
  capture.begin(kBufBytes);
  capture.reset();
  capture.setRecording(true);
  g_head = g_tail = 0;
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  g_channel = 1;
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  g_shHop = millis();
  return true;
}

void wifiHandshakeStreamPoll(const StrikeEmit& emit) {
  while (g_tail != g_head) {
    Hit h;
    memcpy(&h, (const void*)&g_ring[g_tail], sizeof(h));
    g_tail = (g_tail + 1) & (kRing - 1);

    SHAp* ap = nullptr;
    for (auto& x : g_shAps)
      if (memcmp(x.bssid, h.bssid, 6) == 0) {
        ap = &x;
        break;
      }
    if (!ap) {
      if (g_shAps.size() >= 32) continue;
      g_shAps.push_back(SHAp{});
      ap = &g_shAps.back();
      memcpy(ap->bssid, h.bssid, 6);
      ap->ssid[0] = '\0';
      ap->eapol = 0;
      ap->announced = false;
    }
    if (h.ssid[0]) {
      strncpy(ap->ssid, h.ssid, sizeof(ap->ssid) - 1);
      ap->ssid[sizeof(ap->ssid) - 1] = '\0';
    }
    if (!h.eapol) continue;

    if (ap->eapol < 255) ap->eapol++;
    char b[18];
    bssidStr(ap->bssid, b);
    JsonDocument d;
    d["bssid"] = b;
    d["ssid"] = ap->ssid;
    d["rssi"] = h.rssi;
    d["eapol"] = ap->eapol;
    emit("eapol", d);

    if (ap->eapol >= 4 && !ap->announced) {
      ap->announced = true;
      JsonDocument hs;
      hs["bssid"] = b;
      hs["ssid"] = ap->ssid;
      hs["frames"] = capture.frames();
      hs["bytes"] = (uint32_t)capture.used();
      emit("handshake", hs);
    }
  }

  const uint32_t now = millis();
  if (now - g_shHop > 400) {
    g_shHop = now;
    g_channel = g_channel % 11 + 1;
    esp_wifi_set_channel(g_channel, WIFI_SECOND_CHAN_NONE);
  }
}

void wifiHandshakeStreamStop() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  capture.setRecording(false);
  WiFi.mode(WIFI_OFF);
  g_shAps.clear();
}

void WifiHandshakeScreen::draw() {
  char t[44];
  std::snprintf(t, sizeof(t), "Handshake  ch%d  %uKB%s", ch_,
                (unsigned)(capture.used() / 1024), capture.full() ? " FULL" : "");
  kit::begin(t, toolsetAccent("wifi"));

  if (aps_.empty()) {
    kit::headline("waiting", theme().fgDim);
    kit::end("B export   B- back");
    return;
  }
  if (top_ >= (int)aps_.size()) top_ = 0;
  const Ap& ap = aps_[top_];
  const bool hs = ap.eapol >= 4;
  char sub[48];
  std::snprintf(sub, sizeof(sub), "%02X%02X%02X  %d dBm  e%u%s",
                ap.bssid[3], ap.bssid[4], ap.bssid[5], ap.rssi, ap.eapol,
                hs ? "  HS!" : "");
  kit::detail(ap.ssid.c_str(), sub, top_ + 1, (int)aps_.size());
  kit::end("< >   B export   B- back");
}
