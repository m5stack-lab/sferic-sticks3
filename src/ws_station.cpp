#include "ws_station.h"

#ifdef SFERIC_WS

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

#include "dispatch.h"
#include "emit.h"
#include "hal.h"
#include "link.h"
#include "stream_hub.h"

#if __has_include("ws_config.h")
#include "ws_config.h"
#else
#warning "src/ws_config.h missing — copy ws_config.h.example. Using placeholders."
#define WS_WIFI_SSID "sferic-setup"
#define WS_WIFI_PASS "sferic-setup"
#define WS_COLLECTOR_HOST "127.0.0.1"
#define WS_COLLECTOR_PORT 8787
#define WS_COLLECTOR_PATH "/station"
#define WS_STATION_TOKEN "unset"
#define WS_USE_TLS 0
#endif

WsStation wsStation;

namespace {

WebSocketsClient g_ws;
bool g_connected = false;

// Frame the transport-neutral op vocabulary as {"type":...} JSON text frames
// (ADR-0009 §2).
class WsLink : public Link {
 public:
  void sendReply(JsonDocument& d) override { send(d, "reply"); }
  void sendStrike(JsonDocument& d) override { send(d, "strike"); }
  void sendEvent(JsonDocument& d) override { send(d, "event"); }

 private:
  static void send(JsonDocument& d, const char* type) {
    d["type"] = type;
    String body;
    body.reserve(measureJson(d) + 1);
    serializeJson(d, body);
    g_ws.sendTXT(body);
  }
};

WsLink g_link;

void sendHello() {
  JsonDocument d;
  d["type"] = "hello";
  d["token"] = WS_STATION_TOKEN;
  dispatch::stationInfo(d["station"].to<JsonObject>());
  String body;
  serializeJson(d, body);
  g_ws.sendTXT(body);
}

void sendPong() {
  g_ws.sendTXT("{\"type\":\"pong\"}");
}

void onText(const uint8_t* payload, size_t len) {
  JsonDocument d;
  if (deserializeJson(d, payload, len) != DeserializationError::Ok) return;
  const char* type = d["type"] | "";
  if (!strcmp(type, "cmd")) {
    dispatch::handle(d.as<JsonObjectConst>(), g_link);
  } else if (!strcmp(type, "welcome")) {
    Serial.printf("[ws] welcome from %s\n", d["collector"] | "?");
  } else if (!strcmp(type, "ping")) {
    sendPong();
  }
  // "pong" — liveness handled by the library heartbeat; nothing to do.
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      g_connected = true;
      Serial.println("[ws] connected");
      sendHello();
      break;
    case WStype_DISCONNECTED:
      if (g_connected) Serial.println("[ws] disconnected");
      g_connected = false;
      // ADR-0009 §3: a dropped socket kills any stream session (radio was
      // released) and any pending emit arm.
      stream::abort();
      emit::disarm();
      break;
    case WStype_TEXT:
      onText(payload, length);
      break;
    case WStype_ERROR:
      Serial.println("[ws] error");
      break;
    default:
      break;  // BIN / PING / PONG / FRAGMENT — not used
  }
}

}  // namespace

void WsStation::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // An empty WS_WIFI_PASS means an open network (this deployment's AP has no
  // passphrase and gates by MAC allowlist) — use the single-arg join.
  if (sizeof(WS_WIFI_PASS) <= 1)
    WiFi.begin(WS_WIFI_SSID);
  else
    WiFi.begin(WS_WIFI_SSID, WS_WIFI_PASS);
  Serial.printf("[ws] sta mac %s, joining '%s', collector %s:%d%s\n",
                hal.macAddress(), WS_WIFI_SSID, WS_COLLECTOR_HOST,
                (int)WS_COLLECTOR_PORT, WS_COLLECTOR_PATH);

  g_ws.setExtraHeaders("Authorization: Bearer " WS_STATION_TOKEN);
#if WS_USE_TLS
  g_ws.beginSSL(WS_COLLECTOR_HOST, WS_COLLECTOR_PORT, WS_COLLECTOR_PATH);
#else
  g_ws.begin(WS_COLLECTOR_HOST, WS_COLLECTOR_PORT, WS_COLLECTOR_PATH);
#endif
  g_ws.onEvent(onWsEvent);
  // TODO(ws): jittered exponential backoff 1s->30s (ADR-0009 §3). The
  // library only does a fixed interval; good enough for milestone 3.
  g_ws.setReconnectInterval(3000);
  // ping every 15s, 3s pong timeout, drop after 2 misses (ADR-0009 §3).
  g_ws.enableHeartbeat(15000, 3000, 2);
}

void WsStation::loop() {
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !wifiWasUp_)
    Serial.printf("[ws] wifi up  %s\n", WiFi.localIP().toString().c_str());
  else if (!up && wifiWasUp_)
    Serial.println("[ws] wifi down");
  wifiWasUp_ = up;

  g_ws.loop();
}

bool WsStation::linkUp() const { return g_connected; }

#endif  // SFERIC_WS
