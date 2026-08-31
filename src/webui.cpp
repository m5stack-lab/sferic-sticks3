#include "webui.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "hal.h"
#include "pcap.h"
#include "registry.h"
#include "webui_html.h"
#include "version.h"

namespace {
constexpr int kTitleH = 18;
constexpr char kSsid[] = "sferic-ui";

DNSServer* g_dns = nullptr;
WebServer* g_web = nullptr;
uint32_t g_reqs = 0;

String jsonEscape(const std::string& s) {
  String o;
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

void sendStatus() {
  ++g_reqs;
  String j = "{";
  j += String("\"name\":\"") + SFERIC_NAME + "\",\"version\":\"" +
       SFERIC_VERSION + "\",\"build\":\"" + SFERIC_BUILD + "\",";
  j += "\"uptime_s\":" + String(millis() / 1000) + ",";
  j += "\"heap\":" + String((uint32_t)hal.freeHeap()) + ",";
  j += "\"batt\":" + String(hal.battPercent()) + ",";
  j += "\"on_usb\":" + String(hal.onUsb() ? "true" : "false") + ",";
  j += "\"capture\":{\"frames\":" + String(capture.frames()) +
       ",\"used\":" + String((uint32_t)capture.used()) +
       ",\"cap\":" + String((uint32_t)capture.capacity()) +
       ",\"recording\":" + String(capture.recording() ? "true" : "false") +
       ",\"full\":" + String(capture.full() ? "true" : "false") + "}";
  j += "}";
  g_web->send(200, "application/json", j);
}

void sendTools() {
  ++g_reqs;
  String j = "[";
  bool first = true;
  for (const auto& t : registry.all()) {
    if (!first) j += ',';
    first = false;
    j += "{\"id\":\"" + jsonEscape(t.id) + "\",\"name\":\"" +
         jsonEscape(t.name) + "\",\"toolset\":\"" + jsonEscape(t.toolset) +
         "\",\"summary\":\"" + jsonEscape(t.summary) + "\",\"enabled\":" +
         (registry.enabled(t.id) ? "true" : "false") +
         ",\"has_run\":" + (t.run ? "true" : "false") + "}";
  }
  j += "]";
  g_web->send(200, "application/json", j);
}

void setTool() {
  ++g_reqs;
  if (g_web->hasArg("id") && g_web->hasArg("on")) {
    registry.setEnabled(g_web->arg("id").c_str(), g_web->arg("on") == "1");
  }
  g_web->send(200, "application/json", "{\"ok\":true}");
}

// GET /api/tools/run?id=<id>[&p=<params>] — the headless launch path
// (ADR-0004 §4). Same dispatch the MCP server will use. The tool returns a
// JSON object; we wrap it in an envelope. Non-Ok outcomes carry the reason
// code from Registry::run and an HTTP status to match.
void runTool() {
  ++g_reqs;
  if (!g_web->hasArg("id")) {
    g_web->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_id\"}");
    return;
  }
  const std::string id = g_web->arg("id").c_str();
  const std::string params = g_web->hasArg("p") ? g_web->arg("p").c_str() : "";
  const RunResult r = registry.run(id, params);

  if (r.outcome == RunOutcome::Ok) {
    String j = "{\"id\":\"" + jsonEscape(id) + "\",\"ok\":true,\"result\":" +
               String(r.body.c_str()) + "}";
    g_web->send(200, "application/json", j);
    return;
  }
  int code = 500;
  if (r.outcome == RunOutcome::NotFound) code = 404;
  else if (r.outcome == RunOutcome::Disabled) code = 409;
  else if (r.outcome == RunOutcome::NoHeadlessPath) code = 501;
  String j = "{\"id\":\"" + jsonEscape(id) + "\",\"ok\":false,\"error\":\"" +
             jsonEscape(r.body) + "\"}";
  g_web->send(code, "application/json", j);
}

void sendPcap() {
  ++g_reqs;
  g_web->sendHeader("Content-Disposition", "attachment; filename=sferic.pcap");
  g_web->setContentLength(capture.used());
  g_web->send(200, "application/vnd.tcpdump.pcap", "");
  g_web->sendContent((const char*)capture.raw(), capture.used());
}

void sendIndex() {
  ++g_reqs;
  g_web->send_P(200, "text/html", kWebUiHtml);
}
}  // namespace

void WebUiScreen::onEnter() {
  dirty_ = true;
  g_reqs = 0;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kSsid);
  const IPAddress ip = WiFi.softAPIP();

  g_dns = new DNSServer();
  g_dns->start(53, "*", ip);

  g_web = new WebServer(80);
  g_web->on("/", sendIndex);
  g_web->on("/api/status", sendStatus);
  g_web->on("/api/tools", sendTools);
  g_web->on("/api/tools/set", HTTP_POST, setTool);
  g_web->on("/api/tools/run", runTool);
  g_web->on("/api/capture.pcap", sendPcap);
  g_web->onNotFound(sendIndex);  // captive
  g_web->begin();
  Serial.printf("[webui] up  http://%s/\n", ip.toString().c_str());
}

void WebUiScreen::onExit() {
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
  Serial.println("[webui] down");
}

void WebUiScreen::tick() {
  if (g_dns) g_dns->processNextRequest();
  if (g_web) g_web->handleClient();

  if (hal.event(Btn::B) == BtnEvent::Hold) {
    pop();
    return;
  }
  hal.event(Btn::A);

  if (hal.nowMs() - lastDraw_ > 500) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = hal.nowMs();
  }
}

void WebUiScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Settings  Web UI");

  int y = kTitleH + 10;
  g.setTextColor(TFT_GREEN, TFT_BLACK);
  g.setCursor(6, y);
  g.printf("AP  %s", kSsid);
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, y + 16);
  g.printf("http://%s/", WiFi.softAPIP().toString().c_str());
  g.setCursor(6, y + 32);
  g.printf("clients %d   reqs %u", WiFi.softAPgetStationNum(),
           (unsigned)g_reqs);
  g.setCursor(6, y + 48);
  g.setTextColor(TFT_YELLOW, TFT_BLACK);
  g.print("WiFi tools paused while up");

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("B(hold) stop + back");
}
