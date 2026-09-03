#pragma once

// RF Pack S3 (pingequa) — shared SPI bus owner for the CC1101 (Sub-GHz) and
// nRF24L01+ (2.4 GHz) radios. ADR-0012. Pin map + bring-up notes:
// docs/hardware/rf-pack-s3.md. Compiled only under -DSFERIC_RF_PACK
// (env:sferic-rfpack).
//
// This module owns the one SPIClass the two radios share. The subghz.* and
// nrf24.* drivers do register logic on top of rfpack::bus() + their own CSN;
// they never call SPIClass::begin() or drive the other radio's lines.

#include <SPI.h>

#include <cstdint>

namespace rfpack {

// --- HAT2 / bottom-header pins (Bruce m5stack-sticks3 board config) ---
constexpr int PIN_SCK = 5;
constexpr int PIN_MISO = 4;  // shared with the BMI270 INT net — see hw doc
constexpr int PIN_MOSI = 6;

constexpr int PIN_CC1101_CSN = 2;
constexpr int PIN_CC1101_GDO0 = 3;

constexpr int PIN_NRF24_CSN = 8;
constexpr int PIN_NRF24_CE = 1;

// Conservative bring-up clock for both radios (raise per datasheet later).
constexpr uint32_t BRINGUP_HZ = 4'000'000;

enum class Radio : uint8_t { None, SubGhz, Nrf24 };

struct Presence {
  bool cc1101 = false;
  bool nrf24 = false;
  bool any() const { return cc1101 || nrf24; }
};

// Init the shared bus once (idempotent). Drives both CSN high, CE low,
// probes each radio, and records presence. Safe to call with no board
// attached — presence() just comes back all-false.
void begin();

SPIClass& bus();
const Presence& presence();
bool present();  // presence().any()

// One-radio-at-a-time guard (ADR-0012 §3). Redundant with the board's SN74
// mutual-exclusion; makes a double-acquire an explicit failure.
bool acquire(Radio r);  // false if another radio is held
void release();
Radio activeRadio();

// --- low-level SPI helpers, CS handled here ---
// A full transaction: assert csn, run `fn(bus)`, deassert. `fn` gets the
// bus with a transaction already open at BRINGUP_HZ, MODE0, MSB-first.
void withChip(int csn, void (*fn)(SPIClass&));

// CC1101: strobe a command, read a config/status register.
uint8_t cc1101Strobe(uint8_t cmd);
uint8_t cc1101ReadReg(uint8_t addr);        // config space
uint8_t cc1101ReadStatus(uint8_t addr);     // status space (burst bit)
void cc1101WriteReg(uint8_t addr, uint8_t val);

// nRF24: read/write a register (1 byte), pulse CE.
uint8_t nrf24ReadReg(uint8_t addr);
void nrf24WriteReg(uint8_t addr, uint8_t val);
void nrf24Ce(bool high);

}  // namespace rfpack
