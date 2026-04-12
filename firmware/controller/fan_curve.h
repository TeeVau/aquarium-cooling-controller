#pragma once

#include <Arduino.h>

struct FanCurvePoint {
  uint8_t pwmPercent;
  uint16_t rpm;
};

namespace FanCurve {

constexpr uint8_t kStartPwmPercent = 12;
constexpr uint8_t kMinimumHoldPwmPercent = 10;
constexpr uint8_t kMinimumPlausibilityPwmPercent = 10;
constexpr uint8_t kPlausibilityTolerancePercent = 12;

size_t pointCount();
const FanCurvePoint* points();

uint16_t expectedRpmForPwm(uint8_t pwmPercent);
uint16_t rpmToleranceForExpected(uint16_t expectedRpm);
bool isPlausibilityRegion(uint8_t pwmPercent);

}  // namespace FanCurve
