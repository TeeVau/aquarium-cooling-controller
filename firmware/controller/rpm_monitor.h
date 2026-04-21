#pragma once

/**
 * @file rpm_monitor.h
 * @brief Tachometer pulse counting and RPM sampling for the cooling fan.
 */

#include <Arduino.h>

/**
 * @brief Configuration for fan tachometer sampling.
 */
struct RpmMonitorConfig {
  uint8_t tachPin;              ///< GPIO connected to the fan tachometer output.
  uint8_t pulsesPerRevolution;  ///< Tachometer pulses emitted per full fan revolution.
  uint32_t sampleWindowMs;      ///< Minimum sampling window in milliseconds.
};

/**
 * @brief Default tachometer settings for the configured cooling fan.
 */
constexpr RpmMonitorConfig kDefaultRpmMonitorConfig = {
    26,
    2,
    1000,
};

/**
 * @brief Measures fan speed from tachometer pulses.
 *
 * The monitor attaches an interrupt to the tachometer pin, counts falling
 * edges, and converts each completed sample window to revolutions per minute.
 * Only one active instance is supported because the interrupt trampoline is
 * shared.
 */
class RpmMonitor {
 public:
  /**
   * @brief Creates an RPM monitor with the supplied tachometer configuration.
   *
   * @param config Tachometer pin, pulses-per-revolution, and sample window.
   */
  explicit RpmMonitor(const RpmMonitorConfig& config = kDefaultRpmMonitorConfig);

  /**
   * @brief Initializes the tachometer pin and attaches the interrupt handler.
   *
   * @return True when initialization completed.
   */
  bool begin();

  /**
   * @brief Updates the measured RPM after the configured sample window elapses.
   *
   * Call this regularly from the main loop with the current `millis()` value.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void update(uint32_t nowMs);

  /**
   * @brief Returns the most recently calculated fan speed.
   *
   * @return Last measured RPM, rounded to the nearest integer.
   */
  uint16_t rpm() const;

  /**
   * @brief Returns the number of pulses accumulated in the current sample.
   *
   * @return Unsampled tachometer pulse count since the last RPM calculation.
   */
  uint32_t pulseCount() const;

  /**
   * @brief Calculates the age of the current RPM sample.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @return Elapsed milliseconds since the last completed sample.
   */
  uint32_t sampleAgeMs(uint32_t nowMs) const;

 private:
  /**
   * @brief Interrupt trampoline for the active RPM monitor instance.
   */
  static void IRAM_ATTR handleInterrupt();

  /**
   * @brief Records one tachometer pulse from interrupt context.
   */
  void onPulse();

  /**
   * @brief Atomically reads and clears the current pulse counter.
   *
   * @return Pulse count captured for the completed sample window.
   */
  uint32_t takePulseCountSnapshot();

  static RpmMonitor* activeInstance_;

  RpmMonitorConfig config_;
  volatile uint32_t pulseCountSinceSample_;
  uint16_t lastMeasuredRpm_;
  uint32_t lastSampleMs_;
  bool initialized_;
};
