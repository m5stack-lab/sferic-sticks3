#include <Arduino.h>

#include "hal.h"
#include "menu_tree.h"
#include "ui.h"

void setup() {
  hal.begin();
  Serial.begin(115200);
  ui.push(makeRootMenu());
  Serial.println("\n[sferic] up  (serial nav: a=next A=prev b=select B=back)");
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
