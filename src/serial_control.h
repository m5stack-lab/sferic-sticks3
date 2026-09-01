#pragma once

#include <cstddef>

// Headless command channel over USB serial — the milestone-2 station<->collector
// transport (ADR-0003 §6, ADR-0008 §1). It shares the UART with human log
// output: a command is one line beginning with '>', a reply one line beginning
// with '<'; the collector ignores every other line. Bare `a`/`A`/`b`/`B` bytes
// still drive the menu for desk testing (forwarded to hal.injectNav).
//
// Grammar (compact JSON, one per line):
//   >{"id":<int>,"op":"hello"}                         -> capability declaration
//   >{"id":<int>,"op":"status"}                        -> uptime/heap/batt/capture
//   >{"id":<int>,"op":"tools"}                         -> the registry
//   >{"id":<int>,"op":"run","tool":"<id>","params":{}} -> Registry::run dispatch
//   >{"id":<int>,"op":"set","tool":"<id>","enabled":<bool>}
//   <{"id":<int>,"ok":true,...}  |  <{"id":<int>,"ok":false,"error":"<code>"}
class SerialControl {
 public:
  void poll();  // call once per loop()

 private:
  static constexpr size_t kMaxLine = 1024;
  char buf_[kMaxLine];
  size_t len_ = 0;
  bool inCmd_ = false;

  void handleLine();
};

extern SerialControl serialCtl;
