#include <Arduino.h>

#include "hal.h"
#include "menu_tree.h"
#include "registry.h"
#include "ui.h"

void setup() {
  hal.begin();
  Serial.begin(115200);
  registry.begin();        // load per-tool enabled state from NVS
  registerBuiltinTools();  // ADR-0004
  ui.push(makeRootMenu());
  Serial.printf("\n[sferic] up  %d tools\n", (int)registry.all().size());
  for (const auto& t : registry.all())
    Serial.printf("[tool] %-12s %s\n", t.id.c_str(),
                  registry.enabled(t.id) ? "on" : "off");
}

void loop() {
  hal.update();
  ui.tick();
  if (ui.empty()) ui.push(makeRootMenu());  // never fall through to a blank screen

  static uint32_t beat = 0;
  if (millis() - beat > 2000) {
    beat = millis();
    Serial.printf("[beat] up=%lus heap=%uKB\n", millis() / 1000,
                  (unsigned)(hal.freeHeap() / 1024));
  }
  delay(16);
}
