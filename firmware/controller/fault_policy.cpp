/**
 * @file fault_policy.cpp
 * @brief Implements alarm selection and fault response policy.
 */

#include "fault_policy.h"

namespace {

AlarmCode determineAlarmCode(bool waterSensorOk, bool fanOk) {
  if (waterSensorOk && fanOk) {
    return AlarmCode::kNone;
  }

  if (!waterSensorOk && !fanOk) {
    return AlarmCode::kWaterSensorAndFanFault;
  }

  if (!waterSensorOk) {
    return AlarmCode::kWaterSensorFault;
  }

  return AlarmCode::kFanFault;
}

FaultResponse determineResponse(bool waterSensorOk, bool fanOk) {
  if (!waterSensorOk && !fanOk) {
    return FaultResponse::kWaterFallbackAndReportFanFault;
  }

  if (!waterSensorOk) {
    return FaultResponse::kWaterFallback;
  }

  if (!fanOk) {
    return FaultResponse::kReportFanFault;
  }

  return FaultResponse::kNormalControl;
}

FaultSeverity determineSeverity(bool waterSensorOk, bool fanOk) {
  if (!waterSensorOk || !fanOk) {
    return FaultSeverity::kCritical;
  }

  return FaultSeverity::kNone;
}

}  // namespace

namespace FaultPolicy {

FaultPolicySnapshot evaluate(const ControlSnapshot& controlSnapshot,
                             const FaultMonitorSnapshot& faultSnapshot) {
  const bool waterSensorOk = controlSnapshot.waterSensorValid;
  const bool fanOk = !faultSnapshot.faultLatched;
  const AlarmCode alarmCode = determineAlarmCode(waterSensorOk, fanOk);
  const FaultResponse response = determineResponse(waterSensorOk, fanOk);
  FaultSeverity severity = determineSeverity(waterSensorOk, fanOk);

  return {
      alarmCode,
      severity,
      response,
      waterSensorOk,
      fanOk,
      !waterSensorOk || !fanOk,
      alarmCode != AlarmCode::kNone,
      controlSnapshot.finalPwmPercent,
  };
}

const char* alarmCodeLabel(AlarmCode alarmCode) {
  switch (alarmCode) {
    case AlarmCode::kNone:
      return "none";
    case AlarmCode::kWaterSensorFault:
      return "water-sensor-fault";
    case AlarmCode::kFanFault:
      return "fan-fault";
    case AlarmCode::kWaterSensorAndFanFault:
      return "water-sensor+fan-fault";
    default:
      return "unknown";
  }
}

const char* severityLabel(FaultSeverity severity) {
  switch (severity) {
    case FaultSeverity::kNone:
      return "none";
    case FaultSeverity::kWarning:
      return "warning";
    case FaultSeverity::kCritical:
      return "critical";
    default:
      return "unknown";
  }
}

const char* responseLabel(FaultResponse response) {
  switch (response) {
    case FaultResponse::kNormalControl:
      return "normal-control";
    case FaultResponse::kWaterFallback:
      return "water-fallback";
    case FaultResponse::kReportFanFault:
      return "report-fan-fault";
    case FaultResponse::kWaterFallbackAndReportFanFault:
      return "water-fallback+report-fan-fault";
    default:
      return "unknown";
  }
}

}  // namespace FaultPolicy
