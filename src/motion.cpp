#include "motion.h"

#include <M5Unified.h>
#include <math.h>

#include "hal.h"

namespace {
constexpr int kTitleH = 18;
// accel deviation from 1g, in g, per sensitivity (low / med / high)
const float kAccelThresh[3] = {0.35f, 0.16f, 0.07f};
const float kGyroThresh[3] = {80.0f, 40.0f, 18.0f};  // deg/s
const char* kName[3] = {"low", "med", "high"};
}  // namespace

void MotionScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    sens_ = (sens_ + 1) % 3;
    dirty_ = true;
  } else if (b == BtnEvent::Click) {
    events_ = 0;
    dirty_ = true;
  }

  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  M5.Imu.getAccel(&ax, &ay, &az);
  M5.Imu.getGyro(&gx, &gy, &gz);
  accelDev_ = fabsf(sqrtf(ax * ax + ay * ay + az * az) - 1.0f);
  gyroMag_ = sqrtf(gx * gx + gy * gy + gz * gz);

  const uint32_t now = hal.nowMs();
  const bool trig =
      accelDev_ > kAccelThresh[sens_] || gyroMag_ > kGyroThresh[sens_];
  if (trig && now - lastTrig_ > 400) {
    events_++;
    lastTrig_ = now;
    Serial.printf("[motion] event %lu  aDev=%.2f gyro=%.0f (%s)\n",
                  (unsigned long)events_, accelDev_, gyroMag_, kName[sens_]);
  }

  if (now - lastDraw_ > 100) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void MotionScreen::draw() {
  auto& g = hal.gfx();
  const uint32_t now = hal.nowMs();
  const bool moving = (now - lastTrig_) < 700 && events_ > 0;

  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.printf("Motion / Tamper  [%s]", kName[sens_]);

  int y = kTitleH + 8;
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, y);
  g.printf("accel dev  %.3f g", accelDev_);
  g.setCursor(6, y + 14);
  g.printf("gyro       %.0f deg/s", gyroMag_);

  g.setTextSize(2);
  g.setCursor(6, y + 34);
  if (moving) {
    g.setTextColor(TFT_RED, TFT_BLACK);
    g.print("MOTION");
  } else {
    g.setTextColor(TFT_GREEN, TFT_BLACK);
    g.print("QUIET");
  }
  g.setTextSize(1);
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, y + 58);
  if (events_ > 0)
    g.printf("events %lu   last %lus ago", (unsigned long)events_,
             (unsigned long)((now - lastTrig_) / 1000));
  else
    g.print("events 0");

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A sens  B reset  B(hold) back");
}
