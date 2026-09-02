#include "tools_screen.h"

#include <cstdio>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"
#include "registry.h"

namespace {
int visibleRows() { return kit::listRows(); }
}  // namespace

void ToolsScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  const auto& tools = registry.all();
  const int n = tools.size();

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (n == 0) {
    if (dirty_) {
      draw();
      dirty_ = false;
    }
    return;
  }
  if (a == BtnEvent::Click) {
    sel_ = (sel_ + 1) % n;
    dirty_ = true;
  } else if (a == BtnEvent::Hold) {
    sel_ = (sel_ - 1 + n) % n;
    dirty_ = true;
  } else if (b == BtnEvent::Click) {
    const std::string& id = tools[sel_].id;
    registry.setEnabled(id, !registry.enabled(id));
    Serial.printf("[tools] %s -> %s\n", id.c_str(),
                  registry.enabled(id) ? "on" : "off");
    dirty_ = true;
  }

  const int rows = visibleRows();
  if (sel_ < top_) {
    top_ = sel_;
    dirty_ = true;
  }
  if (sel_ >= top_ + rows) {
    top_ = sel_ - rows + 1;
    dirty_ = true;
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void ToolsScreen::draw() {
  const Theme& th = theme();
  const auto& tools = registry.all();

  char t[28];
  std::snprintf(t, sizeof(t), "Tools  %d/%d", sel_ + 1, (int)tools.size());
  kit::begin(t, th.fg);

  const int rows = visibleRows();
  for (int i = 0; i < rows && (top_ + i) < (int)tools.size(); ++i) {
    const int idx = top_ + i;
    const Tool& tl = tools[idx];
    const bool on = registry.enabled(tl.id);
    kit::row(i, tl.name.c_str(), idx == sel_, tl.toolset.c_str(),
             toolsetAccent(tl.toolset.c_str()), on ? 'x' : 'o',
             on ? th.ok : th.fgDim);
  }

  kit::end("A next   A- prev   B toggle   B- back");
}
