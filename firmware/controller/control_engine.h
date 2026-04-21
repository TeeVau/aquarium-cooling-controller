#pragma once

/**
 * @file control_engine.h
 * @brief Computes cooling PWM commands from temperature inputs and policy limits.
 */

#include <Arduino.h>

/**
 * @brief Tunable limits and ramp settings for cooling control.
 */
struct ControlConfig {
  float defaultTargetTemperatureC;     ///< Fallback target water temperature in degrees Celsius.
  float minimumTargetTemperatureC;     ///< Lowest accepted configured target in degrees Celsius.
  float maximumTargetTemperatureC;     ///< Highest accepted configured target in degrees Celsius.
  uint8_t fallbackPwmPercent;          ///< Fan PWM used when water temperature is unavailable.
  float coolingStartDeltaC;            ///< Water delta above target where cooling begins.
  float fullCoolingDeltaC;             ///< Water delta above target where full PWM is requested.
  bool airAssistEnabled;               ///< Enables ambient-air assist when the air sensor is valid.
  float airAssistStartTemperatureC;    ///< Air temperature where assist starts.
  float airAssistFullTemperatureC;     ///< Air temperature where assist reaches its maximum PWM.
  uint8_t airAssistMinimumPwmPercent;  ///< Minimum PWM requested by air assist.
  uint8_t airAssistMaximumPwmPercent;  ///< Maximum PWM requested by air assist.
};

/**
 * @brief Sensor and target inputs for one control calculation.
 */
struct ControlInputs {
  bool hasConfiguredTargetTemperature; ///< True when a user-configured target should be considered.
  float requestedTargetTemperatureC;   ///< Requested target water temperature in degrees Celsius.
  bool waterSensorValid;               ///< True when the water temperature sample is valid.
  float waterTemperatureC;             ///< Water temperature in degrees Celsius.
  bool airSensorValid;                 ///< True when the ambient air temperature sample is valid.
  float airTemperatureC;               ///< Ambient air temperature in degrees Celsius.
};

/**
 * @brief Operating mode selected by the control engine.
 */
enum class ControlMode : uint8_t {
  kWaterControl,              ///< Water sensor drives the final PWM command.
  kWaterControlWithAirAssist, ///< Air-assist demand exceeds the water-based command.
  kWaterSensorFallback,       ///< Water sensor is invalid and fallback PWM is used.
};

/**
 * @brief Complete result of one control calculation.
 */
struct ControlSnapshot {
  float targetTemperatureC;       ///< Effective target water temperature in degrees Celsius.
  bool targetDefaulted;           ///< True when the default target replaced an invalid request.
  bool waterSensorValid;          ///< True when the water temperature value is usable.
  float waterTemperatureC;        ///< Effective water temperature, or NAN when invalid.
  bool airSensorValid;            ///< True when the air temperature value is usable.
  float airTemperatureC;          ///< Effective air temperature, or NAN when invalid.
  float waterDeltaC;              ///< Water temperature minus target temperature.
  uint8_t waterBasedPwmPercent;   ///< PWM requested by water-temperature control.
  uint8_t airBasedPwmPercent;     ///< PWM requested by ambient-air assist.
  uint8_t finalPwmPercent;        ///< Final commanded fan PWM percentage.
  ControlMode mode;               ///< Selected control mode.
};

/**
 * @brief Default control limits for the aquarium cooling firmware.
 */
constexpr ControlConfig kDefaultControlConfig = {
    23.0f,
    15.0f,
    35.0f,
    40,
    0.20f,
    3.00f,
    true,
    26.0f,
    30.0f,
    20,
    45,
};

namespace ControlEngine {

/**
 * @brief Checks whether a target temperature is finite and inside configured limits.
 *
 * @param targetTemperatureC Candidate target temperature in degrees Celsius.
 * @param config Control limits used for validation.
 * @return True when the target can be used.
 */
bool isTargetTemperatureValid(float targetTemperatureC,
                              const ControlConfig& config = kDefaultControlConfig);

/**
 * @brief Replaces invalid target temperatures with the configured default.
 *
 * @param targetTemperatureC Candidate target temperature in degrees Celsius.
 * @param config Control limits and default target.
 * @return A valid target temperature in degrees Celsius.
 */
float sanitizeTargetTemperature(float targetTemperatureC,
                                const ControlConfig& config = kDefaultControlConfig);

/**
 * @brief Computes the fan command and control mode for the current inputs.
 *
 * @param inputs Sensor validity, measured temperatures, and requested target.
 * @param config Control limits and ramp settings.
 * @return Snapshot containing intermediate values and the final PWM command.
 */
ControlSnapshot compute(const ControlInputs& inputs,
                        const ControlConfig& config = kDefaultControlConfig);

/**
 * @brief Converts a control mode to a stable diagnostic label.
 *
 * @param mode Control mode to label.
 * @return Null-terminated label string.
 */
const char* modeLabel(ControlMode mode);

}  // namespace ControlEngine
