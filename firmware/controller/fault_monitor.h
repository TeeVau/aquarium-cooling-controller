#pragma once

/**
 * @file fault_monitor.h
 * @brief Fan RPM plausibility monitor with latched fault recovery.
 *
 * The monitor compares measured tachometer RPM against the expected fan curve
 * after a settling delay. It debounces both failure and recovery so transient
 * fan speed deviations do not immediately toggle the service state.
 */

#include <Arduino.h>

/**
 * @brief Fault detection thresholds for fan RPM plausibility.
 *
 * The mismatch and match counters define how many consecutive samples are
 * required before the monitor changes state. This keeps the policy layer simple:
 * it only needs to consume the latched result from the latest snapshot.
 */
struct FaultMonitorConfig {
  uint8_t mismatchCyclesRequired;         ///< Consecutive bad samples required to latch a fault.
  uint8_t matchCyclesRequiredForRecovery; ///< Consecutive good samples required to clear a latched fault.
  uint32_t settlingTimeMs;                ///< Delay after PWM changes before plausibility is evaluated.
};

/**
 * @brief Diagnostic result from one fan fault-monitor evaluation.
 *
 * This snapshot is designed for both telemetry and policy decisions. It exposes
 * the raw RPM comparison, counter state, and latch state so a user can see
 * whether a fan alarm is caused by an active mismatch or by a fault that is
 * still recovering.
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
 *
 * The class watches for PWM command changes, waits for fan speed to settle, and
 * then compares measured RPM against the interpolated fan curve. A latched fault
 * clears only after enough consecutive plausible samples have been observed.
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
