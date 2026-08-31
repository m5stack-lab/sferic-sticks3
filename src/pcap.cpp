#include "pcap.h"

#include <esp_heap_caps.h>
#include <string.h>

PcapBuffer capture;

namespace {
// radiotap: RATE | CHANNEL | DBM_ANTSIGNAL
constexpr uint8_t kRtLen = 15;

void putLE16(uint8_t* p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}
void putLE32(uint8_t* p, uint32_t v) {
  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}

uint16_t chanFreq(uint8_t ch) {
  if (ch == 14) return 2484;
  return 2407 + ch * 5;
}
}  // namespace

bool PcapBuffer::begin(size_t bytes) {
  if (buf_) return true;
  buf_ = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
  if (!buf_) buf_ = static_cast<uint8_t*>(malloc(bytes));  // fallback
  if (!buf_) return false;
  cap_ = bytes;
  reset();
  return true;
}

void PcapBuffer::writeGlobalHeader() {
  uint8_t* p = buf_;
  putLE32(p + 0, 0xa1b2c3d4);  // magic
  putLE16(p + 4, 2);           // version major
  putLE16(p + 6, 4);           // version minor
  putLE32(p + 8, 0);           // thiszone
  putLE32(p + 12, 0);          // sigfigs
  putLE32(p + 16, 65535);      // snaplen
  putLE32(p + 20, 127);        // LINKTYPE_IEEE802_11_RADIOTAP
  used_ = 24;
}

void PcapBuffer::reset() {
  frames_ = 0;
  full_ = false;
  if (buf_) writeGlobalHeader();
}

bool PcapBuffer::writeFrame(const uint8_t* frame, uint16_t len, int8_t rssi,
                            uint8_t channel, uint64_t ts_us) {
  if (!buf_ || !rec_ || full_) return false;
  const uint32_t caplen = kRtLen + len;
  const size_t need = 16 + caplen;
  if (used_ + need > cap_) {
    full_ = true;
    return false;
  }

  uint8_t* p = buf_ + used_;
  // pcap record header
  putLE32(p + 0, (uint32_t)(ts_us / 1000000ULL));
  putLE32(p + 4, (uint32_t)(ts_us % 1000000ULL));
  putLE32(p + 8, caplen);
  putLE32(p + 12, caplen);
  p += 16;

  // radiotap
  p[0] = 0;  // version
  p[1] = 0;  // pad
  putLE16(p + 2, kRtLen);
  putLE32(p + 4, (1u << 2) | (1u << 3) | (1u << 5));  // RATE|CHANNEL|DBM_ANTSIGNAL
  p[8] = 2;                                           // rate, 500kbps units (1 Mbps)
  p[9] = 0;                                           // pad to 2-byte align channel
  putLE16(p + 10, chanFreq(channel));
  putLE16(p + 12, 0x00A0);  // 2GHz + CCK
  p[14] = (uint8_t)rssi;    // dBm antenna signal
  p += kRtLen;

  memcpy(p, frame, len);
  used_ += need;
  frames_++;
  return true;
}

void PcapBuffer::dumpBase64(Print& out) const {
  static const char* b64 =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  out.printf("---PCAP-BEGIN %u bytes %u frames---\n", (unsigned)used_,
             (unsigned)frames_);
  int col = 0;
  for (size_t i = 0; i < used_; i += 3) {
    uint32_t n = buf_[i] << 16;
    if (i + 1 < used_) n |= buf_[i + 1] << 8;
    if (i + 2 < used_) n |= buf_[i + 2];
    char c[4] = {b64[(n >> 18) & 63], b64[(n >> 12) & 63],
                 (i + 1 < used_) ? b64[(n >> 6) & 63] : '=',
                 (i + 2 < used_) ? b64[n & 63] : '='};
    out.write(c, 4);
    if ((col += 4) >= 76) {
      out.write('\n');
      col = 0;
    }
  }
  if (col) out.write('\n');
  out.println("---PCAP-END---");
}
