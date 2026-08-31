#pragma once

class NimBLEScan;
class NimBLEScanCallbacks;

// NimBLE is initialised once, on first use, and never deinitialised —
// deinit/reinit cycling crashes when a scan callback is in flight. The ~40 KB
// stack stays resident after the first BLE tool runs.

// Lazy init; returns the shared scan handle (scanning stopped).
NimBLEScan* bleScan();

// Stop a running scan and detach callbacks. Safe to call repeatedly.
void bleStop(NimBLEScanCallbacks* cb = nullptr);
