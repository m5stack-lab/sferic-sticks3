#include "mic_spectrum.h"

#include <ArduinoJson.h>
#include <M5Unified.h>
#include <arduinoFFT.h>

#include <cstdio>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kN = 256;                 // FFT size
constexpr uint32_t kRate = 16000;       // Hz
constexpr int kBins = kN / 2;           // usable magnitude bins
constexpr float kBinHz = (float)kRate / kN;

int16_t g_samples[kN];
float g_real[kN];
float g_imag[kN];
float g_mag[kBins];
ArduinoFFT<float> g_fft(g_real, g_imag, kN, (float)kRate);
}  // namespace

void MicSpectrumScreen::onEnter() {
  dirty_ = true;
  M5.Speaker.end();  // speaker + mic share the codec
  M5.Mic.begin();
}

void MicSpectrumScreen::onExit() { M5.Mic.end(); }

void MicSpectrumScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    held_ = !held_;
    dirty_ = true;
  }

  if (!held_ && M5.Mic.isEnabled()) {
    if (M5.Mic.record(g_samples, kN, kRate)) {
      while (M5.Mic.isRecording()) delay(1);
      for (int i = 0; i < kN; ++i) {
        g_real[i] = (float)g_samples[i];
        g_imag[i] = 0.0f;
      }
      g_fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
      g_fft.compute(FFTDirection::Forward);
      g_fft.complexToMagnitude();

      float pk = 0;
      int pkBin = 1;
      for (int i = 1; i < kBins; ++i) {
        g_mag[i] = g_real[i];
        if (g_mag[i] > pk) {
          pk = g_mag[i];
          pkBin = i;
        }
      }
      peakHz_ = pkBin * kBinHz;
      peakMag_ = pk;
      dirty_ = true;

      if (hal.nowMs() - lastLog_ > 500) {
        lastLog_ = hal.nowMs();
        Serial.printf("[mic] peak %.0f Hz  mag %.0f\n", peakHz_, peakMag_);
      }
    }
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void MicSpectrumScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  kit::begin(held_ ? "Mic Spectrum  hold" : "Mic Spectrum",
             toolsetAccent("sensors"));

  const int gx = 4, gy = kit::BODY_Y + 4;
  const int gw = kit::W - 8;
  const int gh = kit::BODY_H - 30;

  // one bar per column, averaging bins into it
  const float norm = peakMag_ > 1.0f ? peakMag_ : 1.0f;
  for (int x = 0; x < gw; ++x) {
    int b0 = (int)((long)x * (kBins - 1) / gw) + 1;
    float m = g_mag[b0 < kBins ? b0 : kBins - 1];
    int h = (int)(gh * (m / norm));
    if (h > gh) h = gh;
    if (h < 0) h = 0;
    uint16_t c = h > gh * 2 / 3 ? th.bad : h > gh / 3 ? th.warn : th.ok;
    g.drawFastVLine(gx + x, gy + gh - h, h, c);
  }

  char l[24];
  std::snprintf(l, sizeof(l), "peak %.0f Hz", peakHz_);
  g.setTextSize(2);
  g.setTextColor(th.fg, th.bg);
  g.setCursor(8, gy + gh + 4);
  g.print(l);

  kit::end("A hold   B- back");
}

// ---------------- headless streaming ----------------

namespace {
uint32_t g_sMicEmit = 0;
}

bool micStreamStart(const std::string&) {
  M5.Speaker.end();  // speaker + mic share the codec
  M5.Mic.begin();
  g_sMicEmit = 0;
  return M5.Mic.isEnabled();
}

void micStreamPoll(const StrikeEmit& emit) {
  if (!M5.Mic.isEnabled()) return;
  if (!M5.Mic.record(g_samples, kN, kRate)) return;
  while (M5.Mic.isRecording()) delay(1);
  for (int i = 0; i < kN; ++i) {
    g_real[i] = (float)g_samples[i];
    g_imag[i] = 0.0f;
  }
  g_fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  g_fft.compute(FFTDirection::Forward);
  g_fft.complexToMagnitude();

  float pk = 0;
  int pkBin = 1;
  for (int i = 1; i < kBins; ++i)
    if (g_real[i] > pk) {
      pk = g_real[i];
      pkBin = i;
    }

  const uint32_t now = millis();
  if (now - g_sMicEmit < 500) return;  // ~2/s
  g_sMicEmit = now;
  JsonDocument d;
  d["peak_hz"] = pkBin * kBinHz;
  d["peak_mag"] = pk;
  emit("mic", d);
}

void micStreamStop() { M5.Mic.end(); }
