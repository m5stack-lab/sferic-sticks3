#include "registry.h"

#include <Preferences.h>

Registry registry;

namespace {
constexpr const char* kNs = "sferic";
constexpr const char* kKey = "tools.disabled";  // comma-separated ids
}  // namespace

void Registry::begin() {
  Preferences p;
  if (!p.begin(kNs, /*readOnly=*/true)) return;
  const String csv = p.getString(kKey, "");
  p.end();

  disabled_.clear();
  int start = 0;
  while (start < (int)csv.length()) {
    int comma = csv.indexOf(',', start);
    if (comma < 0) comma = csv.length();
    String id = csv.substring(start, comma);
    id.trim();
    if (id.length()) disabled_.insert(std::string(id.c_str()));
    start = comma + 1;
  }
}

void Registry::save() const {
  String csv;
  for (const auto& id : disabled_) {
    if (csv.length()) csv += ',';
    csv += id.c_str();
  }
  Preferences p;
  if (!p.begin(kNs, /*readOnly=*/false)) return;
  p.putString(kKey, csv);
  p.end();
}

void Registry::add(Tool t) { tools_.push_back(std::move(t)); }

bool Registry::enabled(const std::string& id) const {
  return disabled_.find(id) == disabled_.end();
}

void Registry::setEnabled(const std::string& id, bool on) {
  if (on)
    disabled_.erase(id);
  else
    disabled_.insert(id);
  save();
}

std::vector<std::string> Registry::toolsets() const {
  std::vector<std::string> out;
  for (const auto& t : tools_) {
    bool seen = false;
    for (const auto& s : out)
      if (s == t.toolset) {
        seen = true;
        break;
      }
    if (!seen) out.push_back(t.toolset);
  }
  return out;
}
