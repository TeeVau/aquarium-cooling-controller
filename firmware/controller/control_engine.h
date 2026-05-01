#pragma once

/**
 * @file control_engine.h
 * @brief Computes cooling PWM commands from temperature inputs and policy limits.
 *
 * This module is intentionally independent from sensors, networking, and PWM
 * hardware. It turns already-sampled temperatures into a deterministic control
 * decision that can be unit-tested without an ESP32 runtime.
 */

#include <Arduino.h>

/**
 * @brief Tunable limits and hysteresis settings for cooling control.
 *
 * The configuration describes the safe operating envelope for water-temperature
 * control. Temperature values are expressed in degrees Celsius and PWM values
 * use the controller-wide 0 to 100 percent scale.
 */
struct ControlConfig {
  float defaultTargetTemperatureC;     ///< Fallback target water temperature in degrees Celsius.
  float minimumTargetTemperatureC;     ///< Lowest accepted configured target in degrees Celsius.
  float maximumTargetTemperatureC;     ///< Highest accepted configured target in degrees Celsius.
  uint8_t fallbackPwmPercent;          ///< Fan PWM used when water temperature is unavailable.
  float coolingOnDeltaC;               ///< Water delta above target where quiet cooling starts.
  float coolingOffDeltaC;              ///< Water delta below target where cooling stops.
  uint8_t quietCoolingPwmPercent;      ///< Fixed PWM used during normal quiet cooling.
};

/**
 * @brief Operating mode selected by the control engine.
 *
 * The mode explains why the final PWM command was selected. It is used in
 * diagnostics and telemetry so users can distinguish between an idle fan
 * state, the fixed quiet cooling stage, and the water-sensor fallback mode.
 */
enum class ControlMode : uint8_t {
  kFanOff,                ///< Water-driven hysteresis currently keeps the fan off.
  kFanLow,                ///< Water-driven hysteresis currently commands the fixed quiet PWM.
  kWaterSensorFallback,   ///< Water sensor is invalid and fallback PWM is used.
};

/**
 * @brief Sensor and target inputs for one control calculation.
 *
 * Inputs are supplied as a snapshot so the control engine does not own sensor
 * polling, persistence, or validation state. Invalid temperature samples remain
 * visible through the validity flags and are converted to fallback behavior by
 * compute().
 */
struct ControlInputs {
  bool hasConfiguredTargetTemperature; ///< True when a user-configured target should be considered.
  float requestedTargetTemperatureC;   ///< Requested target water temperature in degrees Celsius.
  bool waterSensorValid;               ///< True when the water temperature sample is valid.
  float waterTemperatureC;             ///< Water temperature in degrees Celsius.
  bool airSensorValid;                 ///< True when the ambient air temperature sample is valid.
  float airTemperatureC;               ///< Ambient air temperature in degrees Celsius.
  ControlMode previousMode;            ///< Previously active control mode for hysteresis hold behavior.
};

/**
 * @brief Complete result of one control calculation.
 *
 * The snapshot preserves both the final output and the intermediate values used
 * to reach it. Keeping those values together makes serial diagnostics and MQTT
 * telemetry explain the control decision without recalculating it elsewhere.
 */
struct ControlSnapshot {
  float targetTemperatureC;       ///< Effective target water temperature in degrees Celsius.
  bool targetDefaulted;           ///< True when the default target replaced an invalid request.
  bool waterSensorValid;          ///< True when the water temperature value is usable.
  float waterTemperatureC;        ///< Effective water temperature, or NAN when invalid.
  bool airSensorValid;            ///< True when the air temperature value is usable.
  float airTemperatureC;          ///< Effective air temperature, or NAN when invalid.
  float waterDeltaC;              ///< Water temperature minus target temperature.
  uint8_t waterBasedPwmPercent;   ///< PWM requested by water-temperature hysteresis control.
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
    0.5f,
    -0.5f,
    18,
};

namespace ControlEngine {

/**
 * @namespace ControlEngine
 * @brief Pure cooling-control calculations.
 *
 * Functions in this namespace avoid direct hardware access. Callers provide
 * sensor validity and temperature values, and the namespace returns the PWM
 * decision plus stable labels for logs and telemetry.
 */

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
 * Water temperature is the only closed-loop control variable. The previous
 * control mode is used to hold the current state within the hysteresis band.
 * If the water sensor is invalid, the configured fallback PWM is used.
 *
 * @param inputs Sensor validity, measured temperatures, and requested target.
 * @param config Control limits and hysteresis settings.
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
