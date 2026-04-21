/**
 * @file fan_curve.cpp
 * @brief Implements measured fan curve interpolation and tolerance helpers.
 */

#include "fan_curve.h"

namespace {

constexpr FanCurvePoint kMeasuredCurve[] = {
    {0, 0},
    {5, 0},
    // Below the start PWM the hold point is still useful for in-run plausibility.
    {10, 150},
    {12, 187},
    {17, 277},
    {22, 352},
    {27, 435},
    {32, 517},
    {37, 570},
    {42, 637},
    {47, 697},
    {52, 765},
    {57, 832},
    {62, 885},
    {67, 930},
    {72, 997},
    {77, 1050},
    {82, 1102},
    {87, 1147},
    {92, 1185},
    {97, 1230},
    {100, 1252},
};

static_assert(sizeof(kMeasuredCurve) / sizeof(kMeasuredCurve[0]) > 1,
              "Measured fan curve needs at least two points.");

}  // namespace

namespace FanCurve {

size_t pointCount() {
  return sizeof(kMeasuredCurve) / sizeof(kMeasuredCurve[0]);
}

const FanCurvePoint* points() {
  return kMeasuredCurve;
}

uint16_t expectedRpmForPwm(uint8_t pwmPercent) {
  if (pwmPercent <= kMeasuredCurve[0].pwmPercent) {
    return kMeasuredCurve[0].rpm;
  }

  const size_t count = pointCount();
  if (pwmPercent >= kMeasuredCurve[count - 1].pwmPercent) {
    return kMeasuredCurve[count - 1].rpm;
  }

  for (size_t i = 1; i < count; ++i) {
    const FanCurvePoint& lower = kMeasuredCurve[i - 1];
    const FanCurvePoint& upper = kMeasuredCurve[i];

    if (pwmPercent <= upper.pwmPercent) {
      const uint8_t pwmSpan = upper.pwmPercent - lower.pwmPercent;
      const uint16_t rpmSpan = upper.rpm - lower.rpm;
      const uint8_t pwmOffset = pwmPercent - lower.pwmPercent;

      const uint32_t interpolated = lower.rpm +
                                    ((uint32_t)rpmSpan * pwmOffset + (pwmSpan / 2)) /
                                        pwmSpan;
      return (uint16_t)interpolated;
    }
  }

  return kMeasuredCurve[count - 1].rpm;
}

uint16_t rpmToleranceForExpected(uint16_t expectedRpm) {
  if (expectedRpm == 0) {
    return 0;
  }

  const uint32_t tolerance =
      ((uint32_t)expectedRpm * kPlausibilityTolerancePercent + 50U) / 100U;
  return (uint16_t)tolerance;
}

bool isPlausibilityRegion(uint8_t pwmPercent) {
  return pwmPercent >= kMinimumPlausibilityPwmPercent;
}

}  // namespace FanCurve
