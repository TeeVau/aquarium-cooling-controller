#pragma once

#include <Arduino.h>

#include "control_engine.h"
#include "fault_monitor.h"

enum class AlarmCode : uint8_t {
  kNone,
  kWaterSensorFault,
  kAirSensorFault,
  kFanFault,
  kWaterSensorAndFanFault,
  kAirSensorAndFanFault,
  kMultipleFaults,
};

enum class FaultSeverity : uint8_t {
  kNone,
  kWarning,
  kCritical,
};

enum class FaultResponse : uint8_t {
  kNormalControl,
  kDisableAirAssist,
  kWaterFallback,
  kReportFanFault,
  kWaterFallbackAndReportFanFault,
};

struct FaultPolicySnapshot {
  AlarmCode alarmCode;
  FaultSeverity severity;
  FaultResponse response;
  bool waterSensorOk;
  bool airSensorOk;
  bool fanOk;
  bool coolingDegraded;
  bool serviceRequired;
  uint8_t effectivePwmPercent;
};

namespace FaultPolicy {

FaultPolicySnapshot evaluate(const ControlSnapshot& controlSnapshot,
                             const FaultMonitorSnapshot& faultSnapshot);
const char* alarmCodeLabel(AlarmCode alarmCode);
const char* severityLabel(FaultSeverity severity);
const char* responseLabel(FaultResponse response);

}  // namespace FaultPolicy
