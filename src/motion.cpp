#include "motion.h"

#include <M5Unified.h>
#include <math.h>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

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
  const Theme& th = theme();
  const uint32_t now = hal.nowMs();
  const bool moving = (now - lastTrig_) < 700 && events_ > 0;

  char t[40];
  std::snprintf(t, sizeof(t), "Motion / Tamper  [%s]", kName[sens_]);
  kit::begin(t, toolsetAccent("sensors"));
  kit::headline(moving ? "MOTION" : "QUIET", moving ? th.bad : th.ok);

  char l[40];
  std::snprintf(l, sizeof(l), "dev  %.3f g", accelDev_);
  kit::line(0, l);
  std::snprintf(l, sizeof(l), "gyro %.0f d/s", gyroMag_);
  kit::line(1, l);
  if (events_ > 0)
    std::snprintf(l, sizeof(l), "events %lu  %lus", (unsigned long)events_,
                  (unsigned long)((now - lastTrig_) / 1000));
  else
    std::snprintf(l, sizeof(l), "events 0");
  kit::line(2, l, th.fgDim);

  kit::end("A sens   B reset   B- back");
}
