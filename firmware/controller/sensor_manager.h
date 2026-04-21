#pragma once

/**
 * @file sensor_manager.h
 * @brief OneWire temperature sensor discovery, assignment, and sampling.
 */

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

constexpr size_t kMaxTrackedSensors = 2;    ///< Number of named sensors used by control logic.
constexpr size_t kMaxDiscoveredSensors = 4; ///< Maximum discovered OneWire devices retained for diagnostics.

/**
 * @brief Configuration for one named temperature sensor.
 */
struct TrackedSensorConfig {
  bool hasPreferredAddress;      ///< True when preferredAddress should be matched.
  uint8_t preferredAddress[8];   ///< Expected OneWire ROM code for this sensor.
  const char* sensorLabel;       ///< Human-readable label for diagnostics.
};

/**
 * @brief Latest sample and identity data for one tracked sensor.
 */
struct TrackedSensorSnapshot {
  bool configuredAddressMatched; ///< True when the preferred ROM code was found.
  bool addressKnown;             ///< True when romCode contains a discovered address.
  bool sampleValid;              ///< True when temperatureC is usable.
  bool externallyPowered;        ///< True when the device reports external power.
  float temperatureC;            ///< Last sampled temperature in degrees Celsius.
  uint32_t lastSampleMs;         ///< Timestamp of the last valid sample.
  uint8_t romCode[8];            ///< OneWire ROM code assigned to this sensor.
};

/**
 * @brief Diagnostic data for one discovered OneWire sensor.
 */
struct DiscoveredSensorSnapshot {
  bool present;           ///< True when this entry contains a discovered device.
  bool assigned;          ///< True when this device is assigned to a tracked sensor.
  bool externallyPowered; ///< True when the device reports external power.
  uint8_t romCode[8];     ///< OneWire ROM code for the discovered device.
};

/**
 * @brief OneWire bus and temperature sampling configuration.
 */
struct SensorManagerConfig {
  uint8_t oneWirePin;                                      ///< GPIO used for the OneWire bus.
  uint32_t sampleIntervalMs;                               ///< Interval between conversion requests.
  uint8_t resolutionBits;                                  ///< DS18B20 resolution in bits.
  size_t trackedSensorCount;                               ///< Number of configured tracked sensors.
  TrackedSensorConfig trackedSensors[kMaxTrackedSensors];  ///< Named sensor configuration.
};

/**
 * @brief Complete OneWire bus and sensor state snapshot.
 */
struct SensorSnapshot {
  bool busInitialized;                                           ///< True after the OneWire bus is initialized.
  bool presenceDetected;                                         ///< True when at least one device is present.
  bool conversionPending;                                        ///< True while waiting for a temperature conversion.
  uint8_t discoveredSensorCount;                                 ///< Number of devices found during discovery.
  uint32_t lastDiscoveryMs;                                      ///< Timestamp of the last discovery pass.
  uint32_t lastRequestMs;                                        ///< Timestamp of the last conversion request.
  TrackedSensorSnapshot trackedSensors[kMaxTrackedSensors];      ///< State for named sensors.
  DiscoveredSensorSnapshot discoveredSensors[kMaxDiscoveredSensors]; ///< Diagnostic discovery list.
};

/**
 * @brief Discovers and samples DS18B20-compatible sensors on a OneWire bus.
 */
class SensorManager {
 public:
  /**
   * @brief Creates a sensor manager for the supplied OneWire configuration.
   *
   * @param config Bus pin, sampling interval, resolution, and tracked sensors.
   */
  explicit SensorManager(const SensorManagerConfig& config);

  /**
   * @brief Initializes the bus, discovers sensors, and requests the first conversion.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @return True when bus initialization completed.
   */
  bool begin(uint32_t nowMs);

  /**
   * @brief Advances discovery and temperature conversion state.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void update(uint32_t nowMs);

  /**
   * @brief Returns the latest sensor snapshot.
   *
   * @return Immutable snapshot reference.
   */
  const SensorSnapshot& snapshot() const;

  /**
   * @brief Formats a tracked sensor ROM code as a hexadecimal string.
   *
   * @param trackedIndex Index into the tracked sensor array.
   * @param buffer Destination character buffer.
   * @param bufferSize Size of the destination buffer in bytes.
   */
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
