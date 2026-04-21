/**
 * @file control_engine.cpp
 * @brief Implements temperature-based cooling PWM calculations.
 */

#include "control_engine.h"

#include <math.h>

#include "fan_curve.h"

namespace {

uint8_t computeWaterBasedPwm(float waterDeltaC, const ControlConfig& config) {
  if (waterDeltaC <= config.coolingStartDeltaC) {
    return 0;
  }

  if (waterDeltaC >= config.fullCoolingDeltaC) {
    return 100;
  }

  const float rampSpanC = config.fullCoolingDeltaC - config.coolingStartDeltaC;
  const float rampPosition = (waterDeltaC - config.coolingStartDeltaC) / rampSpanC;
  const float pwmRange = 100.0f - FanCurve::kMinimumHoldPwmPercent;
  const float pwmValue =
      FanCurve::kMinimumHoldPwmPercent + rampPosition * pwmRange;

  return (uint8_t)roundf(constrain(pwmValue, 0.0f, 100.0f));
}

uint8_t computeAirBasedPwm(float airTemperatureC, const ControlConfig& config) {
  if (!config.airAssistEnabled || !isfinite(airTemperatureC) ||
      airTemperatureC < config.airAssistStartTemperatureC) {
    return 0;
  }

  if (airTemperatureC >= config.airAssistFullTemperatureC) {
    return config.airAssistMaximumPwmPercent;
  }

  const float assistSpanC =
      config.airAssistFullTemperatureC - config.airAssistStartTemperatureC;
  const float assistPosition =
      (airTemperatureC - config.airAssistStartTemperatureC) / assistSpanC;
  const float pwmRange =
      (float)(config.airAssistMaximumPwmPercent - config.airAssistMinimumPwmPercent);
  const float pwmValue =
      config.airAssistMinimumPwmPercent + assistPosition * pwmRange;

  return (uint8_t)roundf(
      constrain(pwmValue, (float)config.airAssistMinimumPwmPercent,
                (float)config.airAssistMaximumPwmPercent));
}

}  // namespace

namespace ControlEngine {

bool isTargetTemperatureValid(float targetTemperatureC, const ControlConfig& config) {
  return isfinite(targetTemperatureC) &&
         targetTemperatureC >= config.minimumTargetTemperatureC &&
         targetTemperatureC <= config.maximumTargetTemperatureC;
}

float sanitizeTargetTemperature(float targetTemperatureC, const ControlConfig& config) {
  if (isTargetTemperatureValid(targetTemperatureC, config)) {
    return targetTemperatureC;
  }

  return config.defaultTargetTemperatureC;
}

ControlSnapshot compute(const ControlInputs& inputs, const ControlConfig& config) {
  const bool configuredTargetIsValid =
      inputs.hasConfiguredTargetTemperature &&
      isTargetTemperatureValid(inputs.requestedTargetTemperatureC, config);
  const float targetTemperatureC = configuredTargetIsValid
                                       ? inputs.requestedTargetTemperatureC
                                       : config.defaultTargetTemperatureC;
  const bool targetDefaulted = !configuredTargetIsValid;
  ControlSnapshot snapshot = {};
  snapshot.targetTemperatureC = targetTemperatureC;
  snapshot.targetDefaulted = targetDefaulted;
  snapshot.waterSensorValid = inputs.waterSensorValid && isfinite(inputs.waterTemperatureC);
  snapshot.waterTemperatureC =
      snapshot.waterSensorValid ? inputs.waterTemperatureC : NAN;
  snapshot.airSensorValid = inputs.airSensorValid && isfinite(inputs.airTemperatureC);
  snapshot.airTemperatureC = snapshot.airSensorValid ? inputs.airTemperatureC : NAN;
  snapshot.airBasedPwmPercent = snapshot.airSensorValid
                                    ? computeAirBasedPwm(snapshot.airTemperatureC, config)
                                    : 0;

  if (!snapshot.waterSensorValid) {
    snapshot.waterDeltaC = NAN;
    snapshot.waterBasedPwmPercent = 0;
    snapshot.finalPwmPercent = config.fallbackPwmPercent;
    snapshot.mode = ControlMode::kWaterSensorFallback;
    return snapshot;
  }

  const float waterDeltaC = snapshot.waterTemperatureC - targetTemperatureC;
  const uint8_t waterBasedPwmPercent =
      computeWaterBasedPwm(waterDeltaC, config);

  snapshot.waterDeltaC = waterDeltaC;
  snapshot.waterBasedPwmPercent = waterBasedPwmPercent;
  snapshot.finalPwmPercent =
      max(snapshot.waterBasedPwmPercent, snapshot.airBasedPwmPercent);
  snapshot.mode = snapshot.airBasedPwmPercent > snapshot.waterBasedPwmPercent
                      ? ControlMode::kWaterControlWithAirAssist
                      : ControlMode::kWaterControl;
  return snapshot;
}

const char* modeLabel(ControlMode mode) {
  switch (mode) {
    case ControlMode::kWaterControl:
      return "water-control";
    case ControlMode::kWaterControlWithAirAssist:
      return "water-control+air-assist";
    case ControlMode::kWaterSensorFallback:
      return "water-sensor-fallback";
    default:
      return "unknown";
  }
}

}  // namespace ControlEngine
