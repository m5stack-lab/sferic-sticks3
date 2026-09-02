#include "webui.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"
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
       ",\"full\":" + String(capture.full() ? "true" : "false") + "},";
  j += "\"display\":{\"sleep_ms\":" + String((uint32_t)hal.sleepTimeout()) +
       ",\"brightness\":" + String(hal.brightness()) +
       ",\"awake\":" + String(hal.awake() ? "true" : "false") + "}";
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

// POST /api/display?sleep_ms=<ms>&brightness=<0..100> — screen-sleep config,
// the web mirror of Settings > Display. hal.* persists to NVS.
void setDisplay() {
  ++g_reqs;
  if (g_web->hasArg("sleep_ms"))
    hal.setSleepTimeout((uint32_t)g_web->arg("sleep_ms").toInt());
  if (g_web->hasArg("brightness"))
    hal.setBrightness((uint8_t)g_web->arg("brightness").toInt());
  g_web->send(200, "application/json", "{\"ok\":true}");
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
  WiFi.setSleep(false);  // AP modem-sleep makes the SSID flicker in/out of
                         // client scan lists and stalls connects
  const IPAddress ip = WiFi.softAPIP();

  g_dns = new DNSServer();
  g_dns->start(53, "*", ip);

  g_web = new WebServer(80);
  g_web->on("/", sendIndex);
  g_web->on("/api/status", sendStatus);
  g_web->on("/api/tools", sendTools);
  g_web->on("/api/tools/set", HTTP_POST, setTool);
  g_web->on("/api/tools/run", runTool);
  g_web->on("/api/display", HTTP_POST, setDisplay);
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
  const Theme& th = theme();
  kit::begin("Web UI", toolsetAccent("wifi"));
  kit::headline("AP UP", th.ok);

  char l[48];
  std::snprintf(l, sizeof(l), "%s", WiFi.softAPIP().toString().c_str());
  kit::line(0, l);
  std::snprintf(l, sizeof(l), "clients %d", WiFi.softAPgetStationNum());
  kit::line(1, l);
  std::snprintf(l, sizeof(l), "reqs %u", (unsigned)g_reqs);
  kit::line(2, l, th.fgDim);

  kit::end("B- stop + back");
}
