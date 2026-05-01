/**
 * @file control_engine.cpp
 * @brief Implements temperature-based cooling PWM calculations.
 */

#include "control_engine.h"

#include <math.h>

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

  if (!snapshot.waterSensorValid) {
    snapshot.waterDeltaC = NAN;
    snapshot.waterBasedPwmPercent = 0;
    snapshot.finalPwmPercent = config.fallbackPwmPercent;
    snapshot.mode = ControlMode::kWaterSensorFallback;
    return snapshot;
  }

  const float waterDeltaC = snapshot.waterTemperatureC - targetTemperatureC;
  ControlMode mode = inputs.previousMode;
  if (mode != ControlMode::kFanLow && mode != ControlMode::kFanOff) {
    mode = ControlMode::kFanOff;
  }

  if (mode == ControlMode::kFanLow) {
    if (waterDeltaC <= config.coolingOffDeltaC) {
      mode = ControlMode::kFanOff;
    }
  } else if (waterDeltaC >= config.coolingOnDeltaC) {
    mode = ControlMode::kFanLow;
  }

  const uint8_t waterBasedPwmPercent =
      mode == ControlMode::kFanLow ? config.quietCoolingPwmPercent : 0;

  snapshot.waterDeltaC = waterDeltaC;
  snapshot.waterBasedPwmPercent = waterBasedPwmPercent;
  snapshot.finalPwmPercent = snapshot.waterBasedPwmPercent;
  snapshot.mode = mode;
  return snapshot;
}

const char* modeLabel(ControlMode mode) {
  switch (mode) {
    case ControlMode::kFanOff:
      return "fan-off";
    case ControlMode::kFanLow:
      return "fan-low";
    case ControlMode::kWaterSensorFallback:
      return "water-sensor-fallback";
    default:
      return "unknown";
  }
}

}  // namespace ControlEngine
