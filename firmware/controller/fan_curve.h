#pragma once

/**
 * @file fan_curve.h
 * @brief Measured fan PWM-to-RPM curve and plausibility helpers.
 *
 * The curve represents the production fan used by the controller. It is shared
 * by diagnostics and fault detection so expected RPM values are calculated from
 * the same measured data that is shown over serial output.
 */

#include <Arduino.h>

/**
 * @brief One measured point in the fan curve.
 *
 * Points are ordered by increasing PWM and are used for linear interpolation.
 * The first and last entries also act as the clamp values for commands outside
 * the measured range.
 */
struct FanCurvePoint {
  uint8_t pwmPercent; ///< Fan PWM command in percent.
  uint16_t rpm;       ///< Measured fan speed at the PWM command.
};

namespace FanCurve {

/**
 * @namespace FanCurve
 * @brief Fan characterization table and interpolation helpers.
 *
 * The namespace exposes read-only access to measured curve points and helper
 * functions for converting PWM commands into expected RPM and allowed tolerance.
 */

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
 * Values between measured points are linearly interpolated and rounded to the
 * nearest integer RPM. Values outside the curve range are clamped to the nearest
 * measured endpoint.
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
