/**
 * @file fan_driver.cpp
 * @brief Implements ESP32 LEDC fan PWM output control.
 */

#include "fan_driver.h"

FanDriver::FanDriver(const FanDriverConfig& config)
    : config_(config),
      commandedPwmPercent_(0),
      appliedPwmPercent_(0),
      startBoostActive_(false),
      startBoostUntilMs_(0),
      initialized_(false) {}

bool FanDriver::begin() {
  initialized_ = ledcAttach(config_.pwmPin, config_.pwmFrequencyHz,
                            config_.pwmResolutionBits);
  if (!initialized_) {
    return false;
  }

  applyPwmPercent(0);
  return true;
}

void FanDriver::setCommandedPwmPercent(uint8_t pwmPercent, uint32_t nowMs) {
  pwmPercent = min<uint8_t>(pwmPercent, 100);

  if (pwmPercent == commandedPwmPercent_) {
    return;
  }

  const bool wasStopped = commandedPwmPercent_ == 0 && appliedPwmPercent_ == 0;
  commandedPwmPercent_ = pwmPercent;

  if (commandedPwmPercent_ == 0) {
    startBoostActive_ = false;
    startBoostUntilMs_ = 0;
    applyPwmPercent(0);
    return;
  }

  if (wasStopped && commandedPwmPercent_ < config_.startBoostPwmPercent) {
    startBoostActive_ = true;
    startBoostUntilMs_ = nowMs + config_.startBoostDurationMs;
    applyPwmPercent(config_.startBoostPwmPercent);
    return;
  }

  startBoostActive_ = false;
  startBoostUntilMs_ = 0;
  applyPwmPercent(commandedPwmPercent_);
}

void FanDriver::update(uint32_t nowMs) {
  if (!startBoostActive_) {
    return;
  }

  if ((int32_t)(nowMs - startBoostUntilMs_) >= 0) {
    startBoostActive_ = false;
    startBoostUntilMs_ = 0;
    applyPwmPercent(commandedPwmPercent_);
  }
}

uint8_t FanDriver::commandedPwmPercent() const {
  return commandedPwmPercent_;
}

uint8_t FanDriver::appliedPwmPercent() const {
  return appliedPwmPercent_;
}

bool FanDriver::isStartBoostActive() const {
  return startBoostActive_;
}

void FanDriver::applyPwmPercent(uint8_t pwmPercent) {
  pwmPercent = min<uint8_t>(pwmPercent, 100);
  appliedPwmPercent_ = pwmPercent;

  if (!initialized_) {
    return;
  }

  const uint32_t maxDuty = (1UL << config_.pwmResolutionBits) - 1UL;
  const uint32_t duty = (maxDuty * pwmPercent + 50UL) / 100UL;
  ledcWrite(config_.pwmPin, duty);
}
