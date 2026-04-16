#include "fault_policy.h"

namespace {

AlarmCode determineAlarmCode(bool waterSensorOk, bool airSensorOk, bool fanOk) {
  const uint8_t faultCount = (waterSensorOk ? 0 : 1) +
                             (airSensorOk ? 0 : 1) +
                             (fanOk ? 0 : 1);

  if (faultCount == 0) {
    return AlarmCode::kNone;
  }

  if (faultCount > 2 || (!waterSensorOk && !airSensorOk)) {
    return AlarmCode::kMultipleFaults;
  }

  if (!waterSensorOk && !fanOk) {
    return AlarmCode::kWaterSensorAndFanFault;
  }

  if (!airSensorOk && !fanOk) {
    return AlarmCode::kAirSensorAndFanFault;
  }

  if (!waterSensorOk) {
    return AlarmCode::kWaterSensorFault;
  }

  if (!airSensorOk) {
    return AlarmCode::kAirSensorFault;
  }

  return AlarmCode::kFanFault;
}

FaultResponse determineResponse(bool waterSensorOk, bool airSensorOk, bool fanOk) {
  if (!waterSensorOk && !fanOk) {
    return FaultResponse::kWaterFallbackAndReportFanFault;
  }

  if (!waterSensorOk) {
    return FaultResponse::kWaterFallback;
  }

  if (!fanOk) {
    return FaultResponse::kReportFanFault;
  }

  if (!airSensorOk) {
    return FaultResponse::kDisableAirAssist;
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
  const bool airSensorOk = controlSnapshot.airSensorValid;
  const bool fanOk = !faultSnapshot.faultLatched;
  const AlarmCode alarmCode =
      determineAlarmCode(waterSensorOk, airSensorOk, fanOk);
  const FaultResponse response =
      determineResponse(waterSensorOk, airSensorOk, fanOk);
  FaultSeverity severity = determineSeverity(waterSensorOk, fanOk);

  if (severity == FaultSeverity::kNone && !airSensorOk) {
    severity = FaultSeverity::kWarning;
  }

  return {
      alarmCode,
      severity,
      response,
      waterSensorOk,
      airSensorOk,
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
    case AlarmCode::kAirSensorFault:
      return "air-sensor-fault";
    case AlarmCode::kFanFault:
      return "fan-fault";
    case AlarmCode::kWaterSensorAndFanFault:
      return "water-sensor+fan-fault";
    case AlarmCode::kAirSensorAndFanFault:
      return "air-sensor+fan-fault";
    case AlarmCode::kMultipleFaults:
      return "multiple-faults";
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
    case FaultResponse::kDisableAirAssist:
      return "disable-air-assist";
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
