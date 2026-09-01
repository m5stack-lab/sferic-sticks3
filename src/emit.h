#pragma once

#include <cstdint>
#include <functional>
#include <string>

// Two-step arm / fire for emitting tools (IR TX, BLE/USB HID) — the
// MCP/serial mirror of the on-device arm/confirm guard (src/confirm.h,
// ADR-0008 §1). `emit_arm` returns a token + a window; `emit_fire` runs the
// action only with a matching, unexpired token. One pending arm at a time.
//
//   >{"id":N,"op":"emit_arm","tool":"ir.tvbgone","params":{}}
//   <{"id":N,"ok":true,"token":"a1b2c3","expires_ms":60000}
//   >{"id":M,"op":"emit_fire","token":"a1b2c3"}
//   <{"id":M,"ok":true,"result":{"sent":39}}

struct EmitTool {
  const char* id;
  const char* toolset;
  std::function<bool(const std::string& params)> prepare;  // nullable; runs on arm
  std::function<std::string(const std::string& params)> fire;  // returns JSON
};

namespace emit {

void registerTools();  // boot

// arm errors: busy | unknown_tool | capability_disabled | prepare_failed
bool arm(const std::string& id, const std::string& params, std::string& tokenOut,
         uint32_t& windowMsOut, std::string& err);
// fire errors: no_pending | bad_token | expired
bool fire(const std::string& token, std::string& resultOut, std::string& err);

void tick();  // expire a stale arm; call once per loop()
bool armed();
const char* armedId();

}  // namespace emit
