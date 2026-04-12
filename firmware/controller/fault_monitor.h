#pragma once

#include <Arduino.h>

struct FaultMonitorConfig {
  uint8_t mismatchCyclesRequired;
  uint8_t matchCyclesRequiredForRecovery;
  uint32_t settlingTimeMs;
};

struct FaultMonitorSnapshot {
  uint8_t commandedPwmPercent;
  uint16_t measuredRpm;
  uint16_t expectedRpm;
  uint16_t toleranceRpm;
  int16_t rpmError;
  bool plausibilityActive;
  bool plausible;
  bool faultLatched;
  uint8_t mismatchCount;
  uint8_t matchCount;
  uint32_t elapsedSincePwmChangeMs;
};

constexpr FaultMonitorConfig kDefaultFaultMonitorConfig = {
    3,
    3,
    5000,
};

class FaultMonitor {
 public:
  explicit FaultMonitor(
      const FaultMonitorConfig& config = kDefaultFaultMonitorConfig);

  void reset();
  FaultMonitorSnapshot evaluate(uint8_t commandedPwmPercent,
                                uint16_t measuredRpm,
                                uint32_t nowMs);

 private:
  void updateCommandedPwm(uint8_t commandedPwmPercent, uint32_t nowMs);

  FaultMonitorConfig config_;
  uint8_t lastCommandedPwmPercent_;
  uint32_t lastPwmChangeMs_;
  uint8_t mismatchCount_;
  uint8_t matchCount_;
  bool faultLatched_;
  bool hasSeenPwmCommand_;
};
