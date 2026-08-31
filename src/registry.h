#pragma once

#include <set>
#include <string>
#include <vector>

#include "tool.h"

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

  // Toolset ids in first-seen order (only those with >=1 registered tool).
  std::vector<std::string> toolsets() const;

 private:
  std::vector<Tool> tools_;
  std::set<std::string> disabled_;
  void save() const;
};

extern Registry registry;
