#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

constexpr size_t kMaxTrackedSensors = 2;
constexpr size_t kMaxDiscoveredSensors = 4;

struct TrackedSensorConfig {
  bool hasPreferredAddress;
  uint8_t preferredAddress[8];
  const char* sensorLabel;
};

struct TrackedSensorSnapshot {
  bool configuredAddressMatched;
  bool addressKnown;
  bool sampleValid;
  bool externallyPowered;
  float temperatureC;
  uint32_t lastSampleMs;
  uint8_t romCode[8];
};

struct DiscoveredSensorSnapshot {
  bool present;
  bool assigned;
  bool externallyPowered;
  uint8_t romCode[8];
};

struct SensorManagerConfig {
  uint8_t oneWirePin;
  uint32_t sampleIntervalMs;
  uint8_t resolutionBits;
  size_t trackedSensorCount;
  TrackedSensorConfig trackedSensors[kMaxTrackedSensors];
};

struct SensorSnapshot {
  bool busInitialized;
  bool presenceDetected;
  bool conversionPending;
  uint8_t discoveredSensorCount;
  uint32_t lastDiscoveryMs;
  uint32_t lastRequestMs;
  TrackedSensorSnapshot trackedSensors[kMaxTrackedSensors];
  DiscoveredSensorSnapshot discoveredSensors[kMaxDiscoveredSensors];
};

class SensorManager {
 public:
  explicit SensorManager(const SensorManagerConfig& config);

  bool begin(uint32_t nowMs);
  void update(uint32_t nowMs);

  const SensorSnapshot& snapshot() const;
  void formatTrackedAddress(size_t trackedIndex,
                            char* buffer,
                            size_t bufferSize) const;

 private:
  static uint32_t conversionTimeMsForResolution(uint8_t resolutionBits);

  bool discoverSensors(uint32_t nowMs);
  void startConversion(uint32_t nowMs);
  void finishConversion(uint32_t nowMs);
  void clearTrackedSensor(size_t trackedIndex);
  void clearDiscoveredSensors();
  int findDiscoveredIndexByAddress(const uint8_t address[8]) const;

  SensorManagerConfig config_;
  OneWire oneWire_;
  DallasTemperature sensors_;
  SensorSnapshot snapshot_;
  bool initialized_;
  uint32_t conversionReadyAtMs_;
};
