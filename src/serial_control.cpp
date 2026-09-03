#include "serial_control.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "dispatch.h"
#include "hal.h"
#include "link.h"

SerialControl serialCtl;

namespace {

// The USB-CDC framing of the transport-neutral op vocabulary (ADR-0009 §5):
// replies are one '<'-prefixed line, stream strikes/events one '!'-prefixed
// line, sharing the UART with human log output.
class SerialLink : public Link {
 public:
  void sendReply(JsonDocument& d) override { writeLine(d, '<'); }
  void sendStrike(JsonDocument& d) override { writeLine(d, '!'); }
  void sendEvent(JsonDocument& d) override { writeLine(d, '!'); }

 private:
  static void writeLine(const JsonDocument& d, char prefix) {
    // Serialize whole, then one framed write — a byte-streamed multi-KB line
    // can stall on TX backpressure and be read as fragments by the collector.
    String body;
    body.reserve(measureJson(d) + 1);
    serializeJson(d, body);
    Serial.write(prefix);
    Serial.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
    Serial.write('\n');
    Serial.flush();
  }
};

SerialLink g_link;

}  // namespace

void SerialControl::handleLine() {
  buf_[len_] = '\0';
  JsonDocument d;
  if (deserializeJson(d, buf_) != DeserializationError::Ok) {
    JsonDocument e;
    e["id"] = 0;
    e["ok"] = false;
    e["error"] = "bad_json";
    g_link.sendReply(e);
    return;
  }
  dispatch::handle(d.as<JsonObjectConst>(), g_link);
}

void SerialControl::poll() {
  while (Serial.available()) {
    const int ch = Serial.read();

    if (!inCmd_) {
      switch (ch) {
        case '>':
          inCmd_ = true;
          len_ = 0;
          break;
        case 'a':
          hal.injectNav(Btn::A, BtnEvent::Click);
          break;
        case 'A':
          hal.injectNav(Btn::A, BtnEvent::Hold);
          break;
        case 'b':
          hal.injectNav(Btn::B, BtnEvent::Click);
          break;
        case 'B':
          hal.injectNav(Btn::B, BtnEvent::Hold);
          break;
        default:
          break;  // log echo, whitespace, noise — ignore
      }
      continue;
    }

    if (ch == '\n' || ch == '\r') {
      if (len_ > 0) handleLine();
      inCmd_ = false;
      len_ = 0;
      continue;
    }
    if (len_ < kMaxLine - 1) {
      buf_[len_++] = static_cast<char>(ch);
    } else {
      JsonDocument e;
      e["id"] = 0;
      e["ok"] = false;
      e["error"] = "line_overflow";
      g_link.sendReply(e);
      inCmd_ = false;
      len_ = 0;
    }
  }
}
