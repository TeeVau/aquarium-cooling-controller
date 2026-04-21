#pragma once

/**
 * @file fault_monitor.h
 * @brief Fan RPM plausibility monitor with latched fault recovery.
 */

#include <Arduino.h>

/**
 * @brief Fault detection thresholds for fan RPM plausibility.
 */
struct FaultMonitorConfig {
  uint8_t mismatchCyclesRequired;         ///< Consecutive bad samples required to latch a fault.
  uint8_t matchCyclesRequiredForRecovery; ///< Consecutive good samples required to clear a latched fault.
  uint32_t settlingTimeMs;                ///< Delay after PWM changes before plausibility is evaluated.
};

/**
 * @brief Diagnostic result from one fan fault-monitor evaluation.
 */
struct FaultMonitorSnapshot {
  uint8_t commandedPwmPercent;      ///< Current fan PWM command in percent.
  uint16_t measuredRpm;             ///< Latest measured fan speed.
  uint16_t expectedRpm;             ///< Expected RPM from the fan curve.
  uint16_t toleranceRpm;            ///< Allowed absolute RPM deviation.
  int16_t rpmError;                 ///< Measured RPM minus expected RPM.
  bool plausibilityActive;          ///< True when the PWM and settling delay allow checking.
  bool plausible;                   ///< True when measured RPM is inside tolerance.
  bool faultLatched;                ///< True while the monitor has latched a fan fault.
  uint8_t mismatchCount;            ///< Consecutive implausible sample count.
  uint8_t matchCount;               ///< Consecutive plausible sample count while recovering.
  uint32_t elapsedSincePwmChangeMs; ///< Elapsed time since the PWM command changed.
};

/**
 * @brief Default fault-monitor timing and debounce thresholds.
 */
constexpr FaultMonitorConfig kDefaultFaultMonitorConfig = {
    3,
    3,
    5000,
};

/**
 * @brief Tracks fan RPM plausibility and latches persistent fan faults.
 */
class FaultMonitor {
 public:
  /**
   * @brief Creates a fan fault monitor.
   *
   * @param config Fault detection and recovery thresholds.
   */
  explicit FaultMonitor(
      const FaultMonitorConfig& config = kDefaultFaultMonitorConfig);

  /**
   * @brief Clears counters and any latched fan fault.
   */
  void reset();

  /**
   * @brief Evaluates fan RPM plausibility for the current PWM command.
   *
   * @param commandedPwmPercent Current fan PWM command in percent.
   * @param measuredRpm Latest measured fan speed.
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @return Snapshot describing the current plausibility and fault state.
   */
  FaultMonitorSnapshot evaluate(uint8_t commandedPwmPercent,
                                uint16_t measuredRpm,
                                uint32_t nowMs);

 private:
  /**
   * @brief Records PWM command changes and resets plausibility counters.
   *
   * @param commandedPwmPercent Current fan PWM command in percent.
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void updateCommandedPwm(uint8_t commandedPwmPercent, uint32_t nowMs);

  FaultMonitorConfig config_;
  uint8_t lastCommandedPwmPercent_;
  uint32_t lastPwmChangeMs_;
  uint8_t mismatchCount_;
  uint8_t matchCount_;
  bool faultLatched_;
  bool hasSeenPwmCommand_;
};
