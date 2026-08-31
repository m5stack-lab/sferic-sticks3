#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

// A pcap capture buffer in PSRAM. Frames are stored with a radiotap header
// (LINKTYPE_IEEE802_11_RADIOTAP, 127) so Wireshark / aircrack read them
// directly. Export is base64 over serial for now; HTTP download later.
class PcapBuffer {
 public:
  bool begin(size_t bytes);  // allocate in PSRAM; false if unavailable
  void reset();              // clear, keep the buffer, rewrite the global header
  bool recording() const { return rec_; }
  void setRecording(bool on) { rec_ = on; }

  // Append one 802.11 frame. ts_us is a monotonic microsecond count.
  bool writeFrame(const uint8_t* frame, uint16_t len, int8_t rssi,
                  uint8_t channel, uint64_t ts_us);

  size_t used() const { return used_; }
  size_t capacity() const { return cap_; }
  uint32_t frames() const { return frames_; }
  bool full() const { return full_; }

  // Dump the whole buffer base64-framed between markers, for a host to save.
  void dumpBase64(Print& out) const;

 private:
  uint8_t* buf_ = nullptr;
  size_t cap_ = 0;
  size_t used_ = 0;
  uint32_t frames_ = 0;
  bool rec_ = false;
  bool full_ = false;
  void writeGlobalHeader();
};

// One shared capture buffer, used by every WiFi capture tool + Settings>Capture.
extern PcapBuffer capture;
