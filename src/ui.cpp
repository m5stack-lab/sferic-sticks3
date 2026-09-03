#include "ui.h"

#include "theme.h"
#include "ui_kit.h"

ScreenStack ui;

// ---------------- ScreenStack ----------------

void ScreenStack::push(Screen* s) {
  stack_.push_back(s);
  s->onEnter();
}

void ScreenStack::tick() {
  if (stack_.empty()) return;
  Screen* top = stack_.back();
  top->tick();
  if (top->wantsPop()) {
    top->onExit();
    delete top;
    stack_.pop_back();
    if (!stack_.empty()) stack_.back()->onEnter();  // force parent redraw
  }
}

void ScreenStack::reset() {
  for (Screen* s : stack_) {
    s->onExit();
    delete s;
  }
  stack_.clear();
}

namespace {
constexpr int kTitleH = kit::TITLE_H;
}  // namespace

// ---------------- MenuScreen ----------------

MenuScreen::MenuScreen(std::string title, std::vector<MenuItem> items)
    : title_(std::move(title)), items_(std::move(items)) {}

MenuScreen::MenuScreen(std::string title,
                       std::function<std::vector<MenuItem>()> build)
    : title_(std::move(title)), build_(std::move(build)) {}

void MenuScreen::onEnter() {
  if (build_) {
    items_ = build_();
    if (sel_ >= (int)items_.size()) sel_ = items_.empty() ? 0 : items_.size() - 1;
  }
  dirty_ = true;
}

void MenuScreen::tick() {
  if (hal.wokeThisFrame()) dirty_ = true;  // repaint after screen sleep

  switch (hal.nav()) {
    case Nav::Next:
      if (!items_.empty()) {
        sel_ = (sel_ + 1) % (int)items_.size();
        dirty_ = true;
      }
      break;
    case Nav::Prev:
      if (!items_.empty()) {
        sel_ = (sel_ - 1 + (int)items_.size()) % (int)items_.size();
        dirty_ = true;
      }
      break;
    case Nav::Select:
      if (!items_.empty()) {
        items_[sel_].action();  // may push a screen
        return;
      }
      break;
    case Nav::Back:
      pop();
      return;
    case Nav::None:
      break;
  }

  if (dirty_) { draw(); dirty_ = false; }
}

void MenuScreen::draw() {
  kit::statusBar();
  if (items_.empty()) {
    kit::carousel(nullptr, theme().fgDim, "empty");
  } else {
    const MenuItem& it = items_[sel_];
    const char* key = it.icon.empty() ? nullptr : it.icon.c_str();
    kit::carousel(key, key ? toolsetAccent(key) : theme().fg, it.label.c_str());
  }
  kit::hintBar("A >   A-hold <   B open   B-hold back");
}

// ---------------- InfoScreen ----------------

InfoScreen::InfoScreen(std::string title, std::string body)
    : title_(std::move(title)) {
  std::string cur;
  for (char c : body) {
    if (c == '\n') { lines_.push_back(cur); cur.clear(); }
    else cur.push_back(c);
  }
  lines_.push_back(cur);
}

void InfoScreen::tick() {
  if (hal.wokeThisFrame()) dirty_ = true;  // repaint after screen sleep
  const int rows = (hal.h() - kTitleH - 10) / 12;

  switch (hal.nav()) {
    case Nav::Next:  // scroll down, wrapping at the end
      top_ = (top_ + rows < (int)lines_.size()) ? top_ + 1 : 0;
      dirty_ = true;
      break;
    case Nav::Prev:  // scroll up
      if (top_ > 0) { top_--; dirty_ = true; }
      break;
    case Nav::Select:
    case Nav::Back:
      pop();
      return;
    case Nav::None:
      break;
  }
  if (dirty_) { draw(); dirty_ = false; }
}

void InfoScreen::draw() {
  auto& g = hal.gfx();
  const Theme& th = theme();
  kit::begin(title_.c_str(), th.fg);
  const int rows = (hal.h() - kTitleH - 10) / 12;
  g.setTextColor(th.fg, th.bg);
  g.setTextSize(1);
  for (int i = 0; i < rows && (top_ + i) < (int)lines_.size(); ++i) {
    g.setCursor(6, kTitleH + 4 + i * 12);
    g.print(lines_[top_ + i].c_str());
  }
  kit::end((int)lines_.size() > rows ? "A scroll   B back" : "B back");
}
