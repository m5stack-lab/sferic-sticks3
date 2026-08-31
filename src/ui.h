#pragma once

#include <functional>
#include <string>
#include <vector>

#include "hal.h"

// Screen: one full-screen view. A tool subclasses this. tick() handles input
// and drawing for the current frame; call pop() to return to the parent.
class Screen {
 public:
  virtual ~Screen() = default;
  virtual void onEnter() {}  // also called when a child screen pops back to us
  virtual void onExit() {}
  virtual void tick() = 0;

  bool wantsPop() const { return pop_; }
  void clearPop() { pop_ = false; }

 protected:
  void pop() { pop_ = true; }

 private:
  bool pop_ = false;
};

// ScreenStack: owns the pushed screens and deletes them on pop.
class ScreenStack {
 public:
  void push(Screen* s);
  void tick();
  bool empty() const { return stack_.empty(); }
  void reset();  // delete all

 private:
  std::vector<Screen*> stack_;
};

extern ScreenStack ui;

// ---- Menu ----

struct MenuItem {
  std::string label;
  std::function<void()> action;  // run on select; typically ui.push(new ...)
};

class MenuScreen : public Screen {
 public:
  MenuScreen(std::string title, std::vector<MenuItem> items);
  // Live menu: `build` is re-run on every onEnter, so toggles elsewhere
  // (e.g. Settings > Tools) take effect when you return to this screen.
  MenuScreen(std::string title, std::function<std::vector<MenuItem>()> build);
  void onEnter() override;
  void tick() override;

 private:
  std::string title_;
  std::vector<MenuItem> items_;
  std::function<std::vector<MenuItem>()> build_;
  int sel_ = 0;
  int top_ = 0;  // first visible row
  bool dirty_ = true;
  void draw();
};

// Simple scrollable text screen — used for "not implemented yet" + About.
class InfoScreen : public Screen {
 public:
  InfoScreen(std::string title, std::string body);
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  std::string title_;
  std::vector<std::string> lines_;
  int top_ = 0;
  bool dirty_ = true;
  void draw();
};
