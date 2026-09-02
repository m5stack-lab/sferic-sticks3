#include "wifi_deauth.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include <cstdio>
#include <vector>

#include "hal.h"
#include "theme.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kRowH = 12;
constexpr uint32_t kAttackRate = 5;  // deauth/s per BSSID

struct DHit {
  uint8_t bssid[6];
  bool disassoc;
};
constexpr uint8_t kRing = 64;
volatile DHit g_ring[kRing];
volatile uint8_t g_head = 0, g_tail = 0;

void sniff(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  auto* p = static_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint8_t* f = p->payload;
  if (p->rx_ctrl.sig_len < 24) return;
  const uint8_t fc0 = f[0];
  if (((fc0 >> 2) & 0x3) != 0) return;
  const uint8_t sub = (fc0 >> 4) & 0xF;
  if (sub != 0x0C && sub != 0x0A) return;  // deauth / disassoc

  const uint8_t nh = (g_head + 1) & (kRing - 1);
  if (nh == g_tail) return;
  memcpy((void*)g_ring[g_head].bssid, f + 16, 6);  // addr3 = BSSID
  g_ring[g_head].disassoc = (sub == 0x0A);
  g_head = nh;
}
}  // namespace

void WifiDeauthScreen::onEnter() {
  dirty_ = true;
  aps_.clear();
  totalD_ = totalDis_ = 0;
  g_head = g_tail = 0;
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  esp_wifi_set_channel(ch_, WIFI_SECOND_CHAN_NONE);
  lastRate_ = hal.nowMs();
}

void WifiDeauthScreen::onExit() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);
}

void WifiDeauthScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Hold) {
    aps_.clear();
    totalD_ = totalDis_ = 0;
    top_ = 0;
    dirty_ = true;
  } else if (a == BtnEvent::Click) {
    top_++;
    dirty_ = true;
  }

  while (g_tail != g_head) {
    DHit h;
    memcpy(&h, (const void*)&g_ring[g_tail], sizeof(h));
    g_tail = (g_tail + 1) & (kRing - 1);
    Ap* ap = nullptr;
    for (auto& x : aps_)
      if (memcmp(x.bssid, h.bssid, 6) == 0) {
        ap = &x;
        break;
      }
    if (!ap && aps_.size() < 24) {
      aps_.push_back(Ap{});
      ap = &aps_.back();
      memcpy(ap->bssid, h.bssid, 6);
    }
    if (ap) {
      if (h.disassoc) {
        ap->disassoc++;
        totalDis_++;
      } else {
        ap->deauth++;
        totalD_++;
      }
    }
    dirty_ = true;
  }

  const uint32_t now = hal.nowMs();
  if (now - lastRate_ >= 1000) {
    for (auto& ap : aps_) {
      ap.rate = ap.deauth - ap.lastCount;
      ap.lastCount = ap.deauth;
    }
    lastRate_ = now;
    dirty_ = true;
  }
  if (now - lastHop_ > 400) {
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

void WifiDeauthScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  uint32_t peak = 0;
  for (const auto& ap : aps_)
    if (ap.rate > peak) peak = ap.rate;
  const char* v = peak >= kAttackRate ? "ATTACK"
                  : totalD_ > 0        ? "deauth seen"
                                       : "clear";
  const uint16_t vc = peak >= kAttackRate ? th.bad
                      : totalD_ > 0       ? th.warn
                                          : th.ok;

  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.printf("Deauth  D%u Dis%u  ch%d", (unsigned)totalD_, (unsigned)totalDis_,
           ch_);

  g.setTextColor(vc, th.bg);
  g.setTextSize(2);
  g.setCursor(6, kTitleH + 4);
  g.print(v);
  g.setTextSize(1);

  const int rows = (hal.h() - kTitleH - 34) / kRowH;
  if (top_ > (int)aps_.size() - rows) top_ = 0;
  if (top_ < 0) top_ = 0;
  for (int i = 0; i < rows && (top_ + i) < (int)aps_.size(); ++i) {
    const Ap& ap = aps_[top_ + i];
    g.setTextColor(ap.rate >= kAttackRate ? th.bad : th.fg, th.bg);
    g.setCursor(2, kTitleH + 24 + i * kRowH);
    g.printf("%02X%02X%02X%02X%02X%02X d%u/s%u", ap.bssid[0], ap.bssid[1],
             ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5],
             (unsigned)ap.deauth, (unsigned)ap.rate);
  }

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print("A scroll  A(hold) clear  B(hold) back");
}

// ---------------- headless streaming ----------------

namespace {
struct SAp {
  uint8_t bssid[6];
  uint32_t deauth, disassoc, lastCount, rate;
  bool flagged;
};
std::vector<SAp> g_sAps;
uint32_t g_sRate = 0, g_sHop = 0;
uint8_t g_sCh = 1;

void bssidStr(const uint8_t* b, char out[18]) {
  std::snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", b[0], b[1], b[2], b[3],
                b[4], b[5]);
}
}  // namespace

bool wifiDeauthStreamStart(const std::string&) {
  g_sAps.clear();
  g_head = g_tail = 0;
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&sniff);
  g_sCh = 1;
  esp_wifi_set_channel(g_sCh, WIFI_SECOND_CHAN_NONE);
  g_sRate = g_sHop = millis();
  return true;
}

void wifiDeauthStreamPoll(const StrikeEmit& emit) {
  while (g_tail != g_head) {
    DHit h;
    memcpy(&h, (const void*)&g_ring[g_tail], sizeof(h));
    g_tail = (g_tail + 1) & (kRing - 1);
    SAp* ap = nullptr;
    for (auto& x : g_sAps)
      if (memcmp(x.bssid, h.bssid, 6) == 0) {
        ap = &x;
        break;
      }
    if (!ap && g_sAps.size() < 32) {
      g_sAps.push_back(SAp{});
      ap = &g_sAps.back();
      memcpy(ap->bssid, h.bssid, 6);
    }
    if (ap) (h.disassoc ? ap->disassoc : ap->deauth)++;
  }

  const uint32_t now = millis();
  if (now - g_sRate >= 1000) {
    for (auto& ap : g_sAps) {
      ap.rate = ap.deauth - ap.lastCount;
      ap.lastCount = ap.deauth;
      if (ap.rate >= kAttackRate && !ap.flagged) {
        ap.flagged = true;
        char b[18];
        bssidStr(ap.bssid, b);
        JsonDocument d;
        d["bssid"] = b;
        d["deauth"] = ap.deauth;
        d["disassoc"] = ap.disassoc;
        d["rate"] = ap.rate;
        emit("deauth", d);
      } else if (ap.rate < kAttackRate) {
        ap.flagged = false;
      }
    }
    g_sRate = now;
  }
  if (now - g_sHop > 400) {
    g_sHop = now;
    g_sCh = g_sCh % 11 + 1;
    esp_wifi_set_channel(g_sCh, WIFI_SECOND_CHAN_NONE);
  }
}

void wifiDeauthStreamStop() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);
  g_sAps.clear();
}
