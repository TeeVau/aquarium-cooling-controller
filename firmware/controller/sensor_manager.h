#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

struct SensorManagerConfig {
  uint8_t oneWirePin;
  uint32_t sampleIntervalMs;
  uint8_t resolutionBits;
  bool hasPreferredAddress;
  uint8_t preferredAddress[8];
  const char* sensorLabel;
};

struct SensorSnapshot {
  bool busInitialized;
  bool presenceDetected;
  bool sensorDiscovered;
  bool addressKnown;
  bool configuredAddressMatched;
  bool sampleValid;
  bool conversionPending;
  uint8_t discoveredSensorCount;
  float temperatureC;
  uint32_t lastDiscoveryMs;
  uint32_t lastRequestMs;
  uint32_t lastSampleMs;
  uint8_t romCode[8];
};

class SensorManager {
 public:
  explicit SensorManager(const SensorManagerConfig& config);

  bool begin(uint32_t nowMs);
  void update(uint32_t nowMs);

  const SensorSnapshot& snapshot() const;
  void formatPrimaryAddress(char* buffer, size_t bufferSize) const;

 private:
  static uint32_t conversionTimeMsForResolution(uint8_t resolutionBits);

  bool discoverSensor(uint32_t nowMs);
  void startConversion(uint32_t nowMs);
  void finishConversion(uint32_t nowMs);
  void clearAddress();

  SensorManagerConfig config_;
  OneWire oneWire_;
  DallasTemperature sensors_;
  SensorSnapshot snapshot_;
  bool initialized_;
  uint32_t conversionReadyAtMs_;
};
