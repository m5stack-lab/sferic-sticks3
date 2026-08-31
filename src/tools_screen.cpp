#include "tools_screen.h"

#include "hal.h"
#include "registry.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kRowH = 14;
int visibleRows() { return (hal.h() - kTitleH - 10) / kRowH; }
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
  auto& g = hal.gfx();
  const auto& tools = registry.all();

  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Settings  Tools");

  const int rows = visibleRows();
  for (int i = 0; i < rows && (top_ + i) < (int)tools.size(); ++i) {
    const int idx = top_ + i;
    const int y = kTitleH + i * kRowH;
    const Tool& t = tools[idx];
    const bool on = registry.enabled(t.id);
    const bool onSel = (idx == sel_);
    if (onSel) g.fillRect(0, y, hal.w(), kRowH, TFT_NAVY);
    g.setTextColor(onSel ? TFT_WHITE : (on ? TFT_GREEN : TFT_DARKGREY),
                   onSel ? TFT_NAVY : TFT_BLACK);
    g.setCursor(4, y + 3);
    g.printf("[%c] %-6s %s", on ? 'x' : ' ', toolsetLabel(t.toolset),
             t.name.c_str());
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A next  B toggle  B(hold) back");
}
