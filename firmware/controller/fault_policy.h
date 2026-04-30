#pragma once

/**
 * @file fault_policy.h
 * @brief Converts sensor and fan health into alarms, severity, and responses.
 *
 * The policy layer keeps fault interpretation separate from control calculation
 * and raw fan plausibility checks. It turns those lower-level snapshots into
 * user-facing alarm codes and high-level response categories for telemetry.
 */

#include <Arduino.h>

#include "control_engine.h"
#include "fault_monitor.h"

/**
 * @brief Stable alarm identifiers for telemetry and diagnostics.
 *
 * Alarm codes are deliberately coarse and stable. They describe the externally
 * relevant fault combination instead of exposing every internal counter or
 * plausibility detail from the monitor.
 */
enum class AlarmCode : uint8_t {
  kNone,                   ///< No active alarm.
  kWaterSensorFault,       ///< Water sensor is invalid.
  kAirSensorFault,         ///< Air sensor is invalid.
  kFanFault,               ///< Fan RPM plausibility fault is active.
  kWaterSensorAndFanFault, ///< Water sensor and fan faults are active together.
  kAirSensorAndFanFault,   ///< Air sensor and fan faults are active together.
  kMultipleFaults,         ///< Multiple faults are active and need combined handling.
};

/**
 * @brief Severity assigned to the current fault state.
 *
 * Severity expresses how much attention the fault needs. Air-sensor faults are
 * warnings because water control can continue, while water-sensor and fan faults
 * are critical because cooling quality or cooling capacity is degraded.
 */
enum class FaultSeverity : uint8_t {
  kNone,     ///< No service action required.
  kWarning,  ///< Degraded input but cooling can continue.
  kCritical, ///< Cooling is degraded or requires service attention.
};

/**
 * @brief Control response selected for the current fault state.
 *
 * The response is a compact explanation of how the firmware should behave under
 * the current fault combination. It is also published to MQTT so external
 * integrations can react without duplicating firmware policy logic.
 */
enum class FaultResponse : uint8_t {
  kNormalControl,                  ///< Continue normal control behavior.
  kReportAirSensorFault,           ///< Continue control unchanged while reporting the air-sensor fault.
  kWaterFallback,                  ///< Use fallback PWM because water input is invalid.
  kReportFanFault,                 ///< Continue commanding cooling but report fan fault.
  kWaterFallbackAndReportFanFault, ///< Use fallback PWM and report fan fault.
};

/**
 * @brief Complete policy decision for diagnostics and telemetry.
 *
 * The snapshot combines boolean health flags with the selected alarm, severity,
 * and response. It is intentionally redundant enough to make serial output and
 * MQTT payloads understandable without cross-referencing multiple modules.
 */
struct FaultPolicySnapshot {
  AlarmCode alarmCode;        ///< Selected alarm code.
  FaultSeverity severity;     ///< Selected severity.
  FaultResponse response;     ///< Recommended control response.
  bool waterSensorOk;         ///< True when water temperature input is valid.
  bool airSensorOk;           ///< True when air temperature input is valid.
  bool fanOk;                 ///< True when fan plausibility is healthy.
  bool coolingDegraded;       ///< True when cooling capacity or control quality is degraded.
  bool serviceRequired;       ///< True when any alarm should be surfaced to the user.
  uint8_t effectivePwmPercent;///< PWM currently used by the control snapshot.
};

namespace FaultPolicy {

/**
 * @namespace FaultPolicy
 * @brief Fault classification and telemetry labeling helpers.
 *
 * The namespace consumes already-computed control and fan-monitor snapshots.
 * It does not read sensors or drive hardware; it only classifies the current
 * state and returns stable labels for diagnostics.
 */

/**
 * @brief Evaluates alarms and fault response from control and fan-monitor state.
 *
 * @param controlSnapshot Latest control-engine result.
 * @param faultSnapshot Latest fan fault-monitor result.
 * @return Fault policy decision for diagnostics, telemetry, and UI output.
 */
FaultPolicySnapshot evaluate(const ControlSnapshot& controlSnapshot,
                             const FaultMonitorSnapshot& faultSnapshot);

/**
 * @brief Converts an alarm code to a telemetry-safe label.
 *
 * @param alarmCode Alarm code to label.
 * @return Null-terminated label string.
 */
const char* alarmCodeLabel(AlarmCode alarmCode);

/**
 * @brief Converts a severity value to a telemetry-safe label.
 *
 * @param severity Severity to label.
 * @return Null-terminated label string.
 */
const char* severityLabel(FaultSeverity severity);

/**
 * @brief Converts a fault response to a telemetry-safe label.
 *
 * @param response Response to label.
 * @return Null-terminated label string.
 */
const char* responseLabel(FaultResponse response);

}  // namespace FaultPolicy
