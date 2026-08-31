#pragma once

#include <set>
#include <string>
#include <vector>

#include "tool.h"

// Result of a headless dispatch. The non-Ok outcomes map onto the MCP error
// vocabulary (ADR-0003 §3): Disabled -> capability_disabled, NotFound ->
// capability_not_present, NoHeadlessPath -> a device-only tool.
enum class RunOutcome { Ok, NotFound, Disabled, NoHeadlessPath };

struct RunResult {
  RunOutcome outcome;
  std::string body;  // tool output when Ok; a short reason code otherwise
};

// Single source of truth for the tool catalog + per-tool enabled state
// (ADR-0004). The device menu, web UI, and MCP server are all views over
// this.
class Registry {
 public:
  void begin();  // load the disabled set from NVS

  void add(Tool t);
  const std::vector<Tool>& all() const { return tools_; }

  bool enabled(const std::string& id) const;
  void setEnabled(const std::string& id, bool on);  // persists

  // Launch a tool headlessly. Enforces the enabled gate and the
  // has-a-headless-path check in one place so web and MCP behave identically.
  RunResult run(const std::string& id, const std::string& params) const;

  // Toolset ids in first-seen order (only those with >=1 registered tool).
  std::vector<std::string> toolsets() const;

 private:
  std::vector<Tool> tools_;
  std::set<std::string> disabled_;
  void save() const;
};

extern Registry registry;
