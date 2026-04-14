#pragma once

#include <Arduino.h>

struct ControlConfig {
  float defaultTargetTemperatureC;
  float minimumTargetTemperatureC;
  float maximumTargetTemperatureC;
  uint8_t fallbackPwmPercent;
  float coolingStartDeltaC;
  float fullCoolingDeltaC;
};

struct ControlInputs {
  bool hasConfiguredTargetTemperature;
  float requestedTargetTemperatureC;
  bool waterSensorValid;
  float waterTemperatureC;
  bool airSensorValid;
  float airTemperatureC;
};

enum class ControlMode : uint8_t {
  kWaterControl,
  kWaterSensorFallback,
};

struct ControlSnapshot {
  float targetTemperatureC;
  bool targetDefaulted;
  bool waterSensorValid;
  float waterTemperatureC;
  bool airSensorValid;
  float airTemperatureC;
  float waterDeltaC;
  uint8_t waterBasedPwmPercent;
  uint8_t airBasedPwmPercent;
  uint8_t finalPwmPercent;
  ControlMode mode;
};

constexpr ControlConfig kDefaultControlConfig = {
    23.0f,
    15.0f,
    35.0f,
    40,
    0.20f,
    3.00f,
};

namespace ControlEngine {

bool isTargetTemperatureValid(float targetTemperatureC,
                              const ControlConfig& config = kDefaultControlConfig);
float sanitizeTargetTemperature(float targetTemperatureC,
                                const ControlConfig& config = kDefaultControlConfig);
ControlSnapshot compute(const ControlInputs& inputs,
                        const ControlConfig& config = kDefaultControlConfig);
const char* modeLabel(ControlMode mode);

}  // namespace ControlEngine
