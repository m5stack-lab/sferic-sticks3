#include <Arduino.h>

#include "emit.h"
#include "hal.h"
#include "menu_tree.h"
#include "registry.h"
#include "serial_control.h"
#include "stream.h"
#include "ui.h"

void setup() {
  hal.begin();
#if ARDUINO_USB_MODE == 1
  Serial.setTxBufferSize(4096);  // HWCDC only — headless replies are multi-KB
#endif
  Serial.begin(115200);
  registry.begin();        // load per-tool enabled state from NVS
  registerBuiltinTools();  // ADR-0004
  stream::registerTools();  // headless streaming sessions
  emit::registerTools();    // headless emitters (arm/fire)
  ui.push(makeRootMenu());
  Serial.printf("\n[sferic] up  %d tools\n", (int)registry.all().size());
  for (const auto& t : registry.all())
    Serial.printf("[tool] %-12s %s\n", t.id.c_str(),
                  registry.enabled(t.id) ? "on" : "off");
}

void loop() {
  serialCtl.poll();  // headless command channel (collector transport)
  stream::poll();    // active streaming session, if any
  emit::tick();      // expire a stale emit arm
  hal.update();
  ui.tick();
  if (ui.empty()) ui.push(makeRootMenu());  // never fall through to a blank screen

  static uint32_t beat = 0;
  if (millis() - beat > 2000) {
    beat = millis();
    Serial.printf("[beat] up=%lus heap=%uKB awake=%d idle=%lus\n",
                  millis() / 1000, (unsigned)(hal.freeHeap() / 1024),
                  hal.awake() ? 1 : 0, (unsigned long)(hal.idleMs() / 1000));
  }
  delay(16);
}
