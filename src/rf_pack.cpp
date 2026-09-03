#include "rf_pack.h"

#ifdef SFERIC_RF_PACK

#include <Arduino.h>

// All register-level constants and sequences here are written from the
// CC1101 and nRF24L01+ datasheets and are UNVERIFIED — no hardware yet.
// Confirm on first bring-up against docs/hardware/rf-pack-s3.md.

namespace {

SPIClass g_bus(FSPI);
rfpack::Presence g_presence;
rfpack::Radio g_active = rfpack::Radio::None;
bool g_begun = false;

// CC1101 command strobes / header bits
constexpr uint8_t CC1101_WRITE = 0x00;
constexpr uint8_t CC1101_READ = 0x80;
constexpr uint8_t CC1101_BURST = 0x40;
constexpr uint8_t CC1101_SRES = 0x30;   // reset strobe
constexpr uint8_t CC1101_SNOP = 0x3D;   // no-op (returns status byte)
constexpr uint8_t CC1101_PARTNUM = 0x30;  // status reg (read w/ burst bit)
constexpr uint8_t CC1101_VERSION = 0x31;  // status reg; expect 0x14 or 0x17

// nRF24 SPI commands
constexpr uint8_t NRF24_R_REGISTER = 0x00;
constexpr uint8_t NRF24_W_REGISTER = 0x20;
constexpr uint8_t NRF24_NOP = 0xFF;
constexpr uint8_t NRF24_REG_CONFIG = 0x00;
constexpr uint8_t NRF24_REG_EN_AA = 0x01;   // scratch for the presence probe
constexpr uint8_t NRF24_REG_SETUP_AW = 0x03;

SPISettings settings() {
  return SPISettings(rfpack::BRINGUP_HZ, MSBFIRST, SPI_MODE0);
}

void idleLines() {
  pinMode(rfpack::PIN_CC1101_CSN, OUTPUT);
  digitalWrite(rfpack::PIN_CC1101_CSN, HIGH);
  pinMode(rfpack::PIN_NRF24_CSN, OUTPUT);
  digitalWrite(rfpack::PIN_NRF24_CSN, HIGH);
  pinMode(rfpack::PIN_NRF24_CE, OUTPUT);
  digitalWrite(rfpack::PIN_NRF24_CE, LOW);
  pinMode(rfpack::PIN_CC1101_GDO0, INPUT);
}

bool probeCc1101() {
  rfpack::cc1101Strobe(CC1101_SRES);
  delay(5);
  const uint8_t ver = rfpack::cc1101ReadStatus(CC1101_VERSION);
  return ver == 0x14 || ver == 0x17;
}

bool probeNrf24() {
  // Round-trip a scratch register: write a known value, read it back,
  // restore. A floating CSN reads back 0x00 / 0xFF and fails.
  const uint8_t saved = rfpack::nrf24ReadReg(NRF24_REG_EN_AA);
  rfpack::nrf24WriteReg(NRF24_REG_EN_AA, 0x1F);
  const uint8_t a = rfpack::nrf24ReadReg(NRF24_REG_EN_AA);
  rfpack::nrf24WriteReg(NRF24_REG_EN_AA, 0x00);
  const uint8_t b = rfpack::nrf24ReadReg(NRF24_REG_EN_AA);
  rfpack::nrf24WriteReg(NRF24_REG_EN_AA, saved);
  return a == 0x1F && b == 0x00;
}

}  // namespace

namespace rfpack {

void begin() {
  if (g_begun) return;
  g_begun = true;

  idleLines();
  g_bus.begin(PIN_SCK, PIN_MISO, PIN_MOSI);

  // ADR-0012 §4: probe nRF24 first (settles the bus), then CC1101; if the
  // CC1101 probe fails, poke the nRF24 once more and retry.
  g_presence.nrf24 = probeNrf24();
  g_presence.cc1101 = probeCc1101();
  if (!g_presence.cc1101) {
    (void)nrf24ReadReg(NRF24_REG_CONFIG);
    g_presence.cc1101 = probeCc1101();
  }

  Serial.printf("[rfpack] cc1101=%s nrf24=%s\n",
                g_presence.cc1101 ? "yes" : "no",
                g_presence.nrf24 ? "yes" : "no");
}

SPIClass& bus() { return g_bus; }
const Presence& presence() { return g_presence; }
bool present() { return g_presence.any(); }

bool acquire(Radio r) {
  if (g_active != Radio::None && g_active != r) return false;
  g_active = r;
  return true;
}
void release() { g_active = Radio::None; }
Radio activeRadio() { return g_active; }

void withChip(int csn, void (*fn)(SPIClass&)) {
  g_bus.beginTransaction(settings());
  digitalWrite(csn, LOW);
  fn(g_bus);
  digitalWrite(csn, HIGH);
  g_bus.endTransaction();
}

// --- CC1101 ---

uint8_t cc1101Strobe(uint8_t cmd) {
  g_bus.beginTransaction(settings());
  digitalWrite(PIN_CC1101_CSN, LOW);
  const uint8_t st = g_bus.transfer(cmd);
  digitalWrite(PIN_CC1101_CSN, HIGH);
  g_bus.endTransaction();
  return st;
}

uint8_t cc1101ReadReg(uint8_t addr) {
  g_bus.beginTransaction(settings());
  digitalWrite(PIN_CC1101_CSN, LOW);
  g_bus.transfer(addr | CC1101_READ);
  const uint8_t v = g_bus.transfer(0x00);
  digitalWrite(PIN_CC1101_CSN, HIGH);
  g_bus.endTransaction();
  return v;
}

uint8_t cc1101ReadStatus(uint8_t addr) {
  g_bus.beginTransaction(settings());
  digitalWrite(PIN_CC1101_CSN, LOW);
  g_bus.transfer(addr | CC1101_READ | CC1101_BURST);
  const uint8_t v = g_bus.transfer(0x00);
  digitalWrite(PIN_CC1101_CSN, HIGH);
  g_bus.endTransaction();
  return v;
}

void cc1101WriteReg(uint8_t addr, uint8_t val) {
  g_bus.beginTransaction(settings());
  digitalWrite(PIN_CC1101_CSN, LOW);
  g_bus.transfer(addr | CC1101_WRITE);
  g_bus.transfer(val);
  digitalWrite(PIN_CC1101_CSN, HIGH);
  g_bus.endTransaction();
}

// --- nRF24 ---

uint8_t nrf24ReadReg(uint8_t addr) {
  g_bus.beginTransaction(settings());
  digitalWrite(PIN_NRF24_CSN, LOW);
  g_bus.transfer(NRF24_R_REGISTER | (addr & 0x1F));
  const uint8_t v = g_bus.transfer(NRF24_NOP);
  digitalWrite(PIN_NRF24_CSN, HIGH);
  g_bus.endTransaction();
  return v;
}

void nrf24WriteReg(uint8_t addr, uint8_t val) {
  g_bus.beginTransaction(settings());
  digitalWrite(PIN_NRF24_CSN, LOW);
  g_bus.transfer(NRF24_W_REGISTER | (addr & 0x1F));
  g_bus.transfer(val);
  digitalWrite(PIN_NRF24_CSN, HIGH);
  g_bus.endTransaction();
}

void nrf24Ce(bool high) { digitalWrite(PIN_NRF24_CE, high ? HIGH : LOW); }

}  // namespace rfpack

#endif  // SFERIC_RF_PACK
