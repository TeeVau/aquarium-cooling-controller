#pragma once

#include <Arduino.h>

struct RpmMonitorConfig {
  uint8_t tachPin;
  uint8_t pulsesPerRevolution;
  uint32_t sampleWindowMs;
};

constexpr RpmMonitorConfig kDefaultRpmMonitorConfig = {
    26,
    2,
    1000,
};

class RpmMonitor {
 public:
  explicit RpmMonitor(const RpmMonitorConfig& config = kDefaultRpmMonitorConfig);

  bool begin();
  void update(uint32_t nowMs);

  uint16_t rpm() const;
  uint32_t pulseCount() const;
  uint32_t sampleAgeMs(uint32_t nowMs) const;

 private:
  static void IRAM_ATTR handleInterrupt();
  void onPulse();
  uint32_t takePulseCountSnapshot();

  static RpmMonitor* activeInstance_;

  RpmMonitorConfig config_;
  volatile uint32_t pulseCountSinceSample_;
  uint16_t lastMeasuredRpm_;
  uint32_t lastSampleMs_;
  bool initialized_;
};
