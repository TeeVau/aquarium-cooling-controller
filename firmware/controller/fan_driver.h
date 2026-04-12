#pragma once

#include <Arduino.h>

struct FanDriverConfig {
  uint8_t pwmPin;
  uint32_t pwmFrequencyHz;
  uint8_t pwmResolutionBits;
  uint8_t startBoostPwmPercent;
  uint32_t startBoostDurationMs;
};

constexpr FanDriverConfig kDefaultFanDriverConfig = {
    25,
    25000,
    8,
    40,
    2000,
};

class FanDriver {
 public:
  explicit FanDriver(const FanDriverConfig& config = kDefaultFanDriverConfig);

  bool begin();
  void setCommandedPwmPercent(uint8_t pwmPercent, uint32_t nowMs);
  void update(uint32_t nowMs);

  uint8_t commandedPwmPercent() const;
  uint8_t appliedPwmPercent() const;
  bool isStartBoostActive() const;

 private:
  void applyPwmPercent(uint8_t pwmPercent);

  FanDriverConfig config_;
  uint8_t commandedPwmPercent_;
  uint8_t appliedPwmPercent_;
  bool startBoostActive_;
  uint32_t startBoostUntilMs_;
  bool initialized_;
};
