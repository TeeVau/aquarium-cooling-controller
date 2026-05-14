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

bool isControlConfigValid(const ControlConfig& config) {
  return isfinite(config.coolingOnDeltaC) &&
         config.coolingOnDeltaC >= config.minimumCoolingOnDeltaC &&
         config.coolingOnDeltaC <= config.maximumCoolingOnDeltaC &&
         isfinite(config.coolingOffDeltaC) &&
         config.coolingOffDeltaC >= config.minimumCoolingOffDeltaC &&
         config.coolingOffDeltaC <= config.maximumCoolingOffDeltaC &&
         config.coolingOffDeltaC < 0.0f &&
         config.coolingOnDeltaC > 0.0f &&
         isfinite(config.highCoolingDeltaC) &&
         config.highCoolingDeltaC >= config.minimumHighCoolingDeltaC &&
         config.highCoolingDeltaC <= config.maximumHighCoolingDeltaC &&
         config.highCoolingDeltaC > config.coolingOnDeltaC &&
         (config.highCoolingDeltaC - config.coolingOnDeltaC) >= 0.1f &&
         config.fanLowPwmPercent >= config.minimumFanLowPwmPercent &&
         config.fanLowPwmPercent <= config.maximumFanLowPwmPercent &&
         config.fanHighPwmPercent >= config.minimumFanHighPwmPercent &&
         config.fanHighPwmPercent <= config.maximumFanHighPwmPercent &&
         config.fanHighPwmPercent > config.fanLowPwmPercent;
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

  if (!snapshot.waterSensorValid) {
    snapshot.waterDeltaC = NAN;
    snapshot.waterBasedPwmPercent = 0;
    snapshot.finalPwmPercent = config.fallbackPwmPercent;
    snapshot.mode = ControlMode::kWaterSensorFallback;
    return snapshot;
  }

  const float waterDeltaC = snapshot.waterTemperatureC - targetTemperatureC;
  ControlMode mode = inputs.previousMode;
  if (mode != ControlMode::kFanLow &&
      mode != ControlMode::kFanHigh &&
      mode != ControlMode::kFanOff) {
    mode = ControlMode::kFanOff;
  }

  if (mode == ControlMode::kFanHigh) {
    if (waterDeltaC < config.highCoolingDeltaC) {
      mode = ControlMode::kFanLow;
    }
  }

  if (mode == ControlMode::kFanLow) {
    if (waterDeltaC <= config.coolingOffDeltaC) {
      mode = ControlMode::kFanOff;
    } else if (waterDeltaC >= config.highCoolingDeltaC) {
      mode = ControlMode::kFanHigh;
    }
  } else if (mode == ControlMode::kFanOff) {
    if (waterDeltaC >= config.highCoolingDeltaC) {
      mode = ControlMode::kFanHigh;
    } else if (waterDeltaC >= config.coolingOnDeltaC) {
      mode = ControlMode::kFanLow;
    }
  }

  const uint8_t waterBasedPwmPercent =
      mode == ControlMode::kFanHigh
          ? config.fanHighPwmPercent
          : (mode == ControlMode::kFanLow ? config.fanLowPwmPercent : 0);

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
    case ControlMode::kFanHigh:
      return "fan-high";
    case ControlMode::kWaterSensorFallback:
      return "water-sensor-fallback";
    default:
      return "unknown";
  }
}

}  // namespace ControlEngine
