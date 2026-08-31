#include "ble_common.h"

#include <NimBLEDevice.h>

namespace {
bool g_inited = false;
}

NimBLEScan* bleScan() {
  if (!g_inited) {
    NimBLEDevice::init("");
    g_inited = true;
  }
  NimBLEScan* s = NimBLEDevice::getScan();
  if (s->isScanning()) s->stop();
  s->clearResults();
  return s;
}

void bleStop(NimBLEScanCallbacks* cb) {
  if (!g_inited) return;
  NimBLEScan* s = NimBLEDevice::getScan();
  if (s->isScanning()) s->stop();
  // detach callbacks so nothing fires into a torn-down screen
  s->setScanCallbacks(nullptr, false);
  (void)cb;
  s->clearResults();
}
