#include "emit.h"

#include <Arduino.h>
#include <esp_system.h>

#include <cstdio>
#include <vector>

#include "ble_hid.h"
#include "ir_tool.h"
#include "ir_tvbgone.h"
#include "registry.h"
#include "usb_hid.h"

namespace {
constexpr uint32_t kWindowMs = 60000;

std::vector<EmitTool> g_tools;
const EmitTool* g_pending = nullptr;
String g_params;
String g_token;
uint32_t g_deadline = 0;

const EmitTool* find(const std::string& id) {
  for (const auto& t : g_tools)
    if (id == t.id) return &t;
  return nullptr;
}
}  // namespace

void emit::registerTools() {
  g_tools.push_back({"ir.tvbgone", "ir", nullptr, &tvBGoneEmit});
  g_tools.push_back({"ir.replay", "ir", nullptr, &irReplayEmit});
  g_tools.push_back({"ble.hid", "ble", &bleHidArm, &bleHidEmit});
#ifdef SFERIC_USB_HID
  g_tools.push_back({"usb.hid", "usb", &usbHidArm, &usbHidEmit});
#endif
}

bool emit::arm(const std::string& id, const std::string& params,
               std::string& tokenOut, uint32_t& windowMsOut, std::string& err) {
  if (g_pending) {
    err = "busy";
    return false;
  }
  const EmitTool* t = find(id);
  if (!t) {
    err = "unknown_tool";
    return false;
  }
  if (!registry.enabled(id)) {
    err = "capability_disabled";
    return false;
  }
  if (t->prepare && !t->prepare(params)) {
    err = "prepare_failed";
    return false;
  }
  char tk[10];
  std::snprintf(tk, sizeof(tk), "%06lx", (unsigned long)(esp_random() & 0xFFFFFF));
  g_token = tk;
  g_params = params.c_str();
  g_pending = t;
  g_deadline = millis() + kWindowMs;
  tokenOut = tk;
  windowMsOut = kWindowMs;
  return true;
}

bool emit::fire(const std::string& token, std::string& resultOut, std::string& err) {
  if (!g_pending) {
    err = "no_pending";
    return false;
  }
  if (millis() > g_deadline) {
    g_pending = nullptr;
    err = "expired";
    return false;
  }
  if (g_token != token.c_str()) {
    err = "bad_token";
    return false;
  }
  const EmitTool* t = g_pending;
  const String p = g_params;
  g_pending = nullptr;
  resultOut = t->fire(p.c_str());
  return true;
}

void emit::tick() {
  if (g_pending && millis() > g_deadline) g_pending = nullptr;
}

bool emit::armed() { return g_pending != nullptr; }
const char* emit::armedId() { return g_pending ? g_pending->id : ""; }
