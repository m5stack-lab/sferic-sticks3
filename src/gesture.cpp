#include "gesture.h"

#include <M5Unified.h>
#include <math.h>

#include "hal.h"
#include "theme.h"

namespace {
constexpr int kTitleH = 18;

const char* orientOf(float ax, float ay, float az) {
  const float x = fabsf(ax), y = fabsf(ay), z = fabsf(az);
  if (z > x && z > y) return az > 0 ? "face up" : "face down";
  if (x > y) return ax > 0 ? "tilt right" : "tilt left";
  return ay > 0 ? "tilt up" : "tilt down";
}
}  // namespace

void GestureScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    shakes_ = 0;
    dirty_ = true;
  }

  float gx = 0, gy = 0, gz = 0;
  M5.Imu.getAccel(&ax_, &ay_, &az_);
  M5.Imu.getGyro(&gx, &gy, &gz);
  gmag_ = sqrtf(gx * gx + gy * gy + gz * gz);
  const float adev = fabsf(sqrtf(ax_ * ax_ + ay_ * ay_ + az_ * az_) - 1.0f);

  const uint32_t now = hal.nowMs();
  const char* newOrient = orientOf(ax_, ay_, az_);

  if (adev > 0.6f && now - lastShake_ > 250) {
    shakes_++;
    lastShake_ = now;
    gesture_ = "shake";
    lastGesture_ = now;
    Serial.printf("[gesture] shake #%lu\n", (unsigned long)shakes_);
  } else if (newOrient != orient_ && now - lastGesture_ > 300) {
    gesture_ = newOrient;
    lastGesture_ = now;
    Serial.printf("[gesture] %s\n", newOrient);
  }
  orient_ = newOrient;

  if (now - lastDraw_ > 120) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void GestureScreen::draw() {
  const Theme& th = theme();
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Sensors  Gesture");

  int y = kTitleH + 6;
  g.setTextSize(2);
  g.setTextColor(th.accentBle, th.bg);
  g.setCursor(6, y);
  g.print(orient_);

  g.setTextSize(1);
  g.setTextColor(th.fg, th.bg);
  g.setCursor(6, y + 24);
  g.printf("a %+.2f %+.2f %+.2f", ax_, ay_, az_);
  g.setCursor(6, y + 38);
  g.printf("gyro %.0f deg/s", gmag_);
  g.setCursor(6, y + 52);
  g.printf("shakes %lu   last: %s", (unsigned long)shakes_, gesture_);

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print("A reset  B(hold) back");
}
