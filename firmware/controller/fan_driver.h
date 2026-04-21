#pragma once

/**
 * @file fan_driver.h
 * @brief PWM fan output driver with low-speed start boost.
 */

#include <Arduino.h>

/**
 * @brief PWM output settings for the cooling fan.
 */
struct FanDriverConfig {
  uint8_t pwmPin;                ///< GPIO used for fan PWM output.
  uint32_t pwmFrequencyHz;       ///< PWM carrier frequency in hertz.
  uint8_t pwmResolutionBits;     ///< PWM resolution passed to ledcAttach().
  uint8_t startBoostPwmPercent;  ///< Temporary PWM applied to start a stopped fan.
  uint32_t startBoostDurationMs; ///< Duration of the start boost in milliseconds.
};

/**
 * @brief Default PWM settings for the configured cooling fan.
 */
constexpr FanDriverConfig kDefaultFanDriverConfig = {
    25,
    25000,
    8,
    40,
    2000,
};

/**
 * @brief Drives the cooling fan with an ESP32 LEDC PWM output.
 */
class FanDriver {
 public:
  /**
   * @brief Creates a fan driver using the supplied PWM configuration.
   *
   * @param config Fan PWM pin, frequency, resolution, and start-boost settings.
   */
  explicit FanDriver(const FanDriverConfig& config = kDefaultFanDriverConfig);

  /**
   * @brief Attaches the PWM channel and initializes the fan output to off.
   *
   * @return True when ESP32 LEDC setup succeeded.
   */
  bool begin();

  /**
   * @brief Sets the desired fan PWM command.
   *
   * A start boost is applied automatically when a stopped fan is commanded to a
   * low nonzero PWM value.
   *
   * @param pwmPercent Desired fan command from 0 to 100 percent.
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void setCommandedPwmPercent(uint8_t pwmPercent, uint32_t nowMs);

  /**
   * @brief Ends the start boost when its configured duration has elapsed.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void update(uint32_t nowMs);

  /**
   * @brief Returns the requested fan PWM command.
   *
   * @return Commanded PWM percentage from 0 to 100.
   */
  uint8_t commandedPwmPercent() const;

  /**
   * @brief Returns the PWM value currently applied to the hardware.
   *
   * @return Applied PWM percentage from 0 to 100.
   */
  uint8_t appliedPwmPercent() const;

  /**
   * @brief Indicates whether the temporary fan start boost is active.
   *
   * @return True while the boost PWM is being applied.
   */
  bool isStartBoostActive() const;

 private:
  /**
   * @brief Writes a clamped PWM percentage to the ESP32 LEDC output.
   *
   * @param pwmPercent PWM percentage to apply.
   */
  void applyPwmPercent(uint8_t pwmPercent);

  FanDriverConfig config_;
  uint8_t commandedPwmPercent_;
  uint8_t appliedPwmPercent_;
  bool startBoostActive_;
  uint32_t startBoostUntilMs_;
  bool initialized_;
};
