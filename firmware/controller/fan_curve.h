#pragma once

/**
 * @file fan_curve.h
 * @brief Measured fan PWM-to-RPM curve and plausibility helpers.
 */

#include <Arduino.h>

/**
 * @brief One measured point in the fan curve.
 */
struct FanCurvePoint {
  uint8_t pwmPercent; ///< Fan PWM command in percent.
  uint16_t rpm;       ///< Measured fan speed at the PWM command.
};

namespace FanCurve {

constexpr uint8_t kStartPwmPercent = 12;                ///< PWM where the fan reliably starts.
constexpr uint8_t kMinimumHoldPwmPercent = 10;          ///< Lowest PWM used after the fan is spinning.
constexpr uint8_t kMinimumPlausibilityPwmPercent = 10;  ///< Minimum PWM where RPM plausibility is checked.
constexpr uint8_t kPlausibilityTolerancePercent = 12;   ///< RPM tolerance around the expected curve.

/**
 * @brief Returns the number of measured fan-curve points.
 *
 * @return Number of entries available from points().
 */
size_t pointCount();

/**
 * @brief Returns the measured fan-curve table.
 *
 * @return Pointer to the first measured curve point.
 */
const FanCurvePoint* points();

/**
 * @brief Interpolates the expected RPM for a PWM command.
 *
 * @param pwmPercent Fan PWM command in percent.
 * @return Expected fan speed in RPM.
 */
uint16_t expectedRpmForPwm(uint8_t pwmPercent);

/**
 * @brief Calculates the tolerated RPM deviation for an expected speed.
 *
 * @param expectedRpm Expected fan speed in RPM.
 * @return Absolute RPM tolerance.
 */
uint16_t rpmToleranceForExpected(uint16_t expectedRpm);

/**
 * @brief Checks whether a PWM command is high enough for RPM plausibility checks.
 *
 * @param pwmPercent Fan PWM command in percent.
 * @return True when RPM plausibility should be evaluated.
 */
bool isPlausibilityRegion(uint8_t pwmPercent);

}  // namespace FanCurve
