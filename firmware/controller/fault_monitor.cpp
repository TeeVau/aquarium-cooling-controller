/**
 * @file fault_monitor.cpp
 * @brief Implements fan RPM plausibility monitoring and fault latching.
 */

#include "fault_monitor.h"

#include "fan_curve.h"

FaultMonitor::FaultMonitor(const FaultMonitorConfig& config) : config_(config) {
  reset();
}

void FaultMonitor::reset() {
  lastCommandedPwmPercent_ = 0;
  lastPwmChangeMs_ = 0;
  mismatchCount_ = 0;
  matchCount_ = 0;
  faultLatched_ = false;
  hasSeenPwmCommand_ = false;
}

void FaultMonitor::updateCommandedPwm(uint8_t commandedPwmPercent,
                                      uint32_t nowMs) {
  if (!hasSeenPwmCommand_ || commandedPwmPercent != lastCommandedPwmPercent_) {
    lastCommandedPwmPercent_ = commandedPwmPercent;
    lastPwmChangeMs_ = nowMs;
    mismatchCount_ = 0;
    matchCount_ = 0;
    hasSeenPwmCommand_ = true;
  }
}

FaultMonitorSnapshot FaultMonitor::evaluate(uint8_t commandedPwmPercent,
                                            uint16_t measuredRpm,
                                            uint32_t nowMs) {
  updateCommandedPwm(commandedPwmPercent, nowMs);

  const uint32_t elapsedSincePwmChangeMs = nowMs - lastPwmChangeMs_;
  const bool settlingComplete = elapsedSincePwmChangeMs >= config_.settlingTimeMs;
  const bool plausibilityActive =
      FanCurve::isPlausibilityRegion(commandedPwmPercent) && settlingComplete;

  const uint16_t expectedRpm =
      FanCurve::expectedRpmForPwm(commandedPwmPercent);
  const uint16_t toleranceRpm =
      FanCurve::rpmToleranceForExpected(expectedRpm);
  const int16_t rpmError = (int16_t)measuredRpm - (int16_t)expectedRpm;

  bool plausible = true;

  if (plausibilityActive) {
    const int32_t absoluteError = abs((int32_t)rpmError);
    plausible = absoluteError <= toleranceRpm;

    if (plausible) {
      mismatchCount_ = 0;

      if (faultLatched_) {
        if (matchCount_ < UINT8_MAX) {
          ++matchCount_;
        }

        if (matchCount_ >= config_.matchCyclesRequiredForRecovery) {
          faultLatched_ = false;
          matchCount_ = 0;
        }
      }
    } else {
      matchCount_ = 0;

      if (mismatchCount_ < UINT8_MAX) {
        ++mismatchCount_;
      }

      if (mismatchCount_ >= config_.mismatchCyclesRequired) {
        faultLatched_ = true;
      }
    }
  } else {
    plausible = true;
    mismatchCount_ = 0;
    matchCount_ = 0;
  }

  return {
      commandedPwmPercent,
      measuredRpm,
      expectedRpm,
      toleranceRpm,
      rpmError,
      plausibilityActive,
      plausible,
      faultLatched_,
      mismatchCount_,
      matchCount_,
      elapsedSincePwmChangeMs,
  };
}
