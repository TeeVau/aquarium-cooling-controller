#include "sensor_manager.h"

#include <string.h>

namespace {

constexpr size_t kRomCodeLength = 8;

}  // namespace

SensorManager::SensorManager(const SensorManagerConfig& config)
    : config_(config),
      oneWire_(config.oneWirePin),
      sensors_(&oneWire_),
      snapshot_{},
      initialized_(false),
      conversionReadyAtMs_(0) {
  snapshot_.temperatureC = NAN;
  clearAddress();
}

bool SensorManager::begin(uint32_t nowMs) {
  sensors_.begin();
  sensors_.setWaitForConversion(false);
  sensors_.setCheckForConversion(false);
  sensors_.setResolution(config_.resolutionBits);

  initialized_ = true;
  snapshot_.busInitialized = true;

  if (discoverSensor(nowMs)) {
    startConversion(nowMs);
  }

  return initialized_;
}

void SensorManager::update(uint32_t nowMs) {
  if (!initialized_) {
    return;
  }

  if (!snapshot_.sensorDiscovered) {
    if (snapshot_.lastDiscoveryMs == 0 ||
        nowMs - snapshot_.lastDiscoveryMs >= config_.sampleIntervalMs) {
      if (discoverSensor(nowMs)) {
        startConversion(nowMs);
      }
    }
    return;
  }

  if (snapshot_.conversionPending) {
    if ((int32_t)(nowMs - conversionReadyAtMs_) >= 0) {
      finishConversion(nowMs);
    }
    return;
  }

  if (snapshot_.lastRequestMs == 0 ||
      nowMs - snapshot_.lastRequestMs >= config_.sampleIntervalMs) {
    startConversion(nowMs);
  }
}

const SensorSnapshot& SensorManager::snapshot() const {
  return snapshot_;
}

void SensorManager::formatPrimaryAddress(char* buffer, size_t bufferSize) const {
  if (bufferSize == 0) {
    return;
  }

  if (!snapshot_.addressKnown || bufferSize < (kRomCodeLength * 2U + 1U)) {
    snprintf(buffer, bufferSize, "n/a");
    return;
  }

  snprintf(buffer, bufferSize,
           "%02X%02X%02X%02X%02X%02X%02X%02X",
           snapshot_.romCode[0],
           snapshot_.romCode[1],
           snapshot_.romCode[2],
           snapshot_.romCode[3],
           snapshot_.romCode[4],
           snapshot_.romCode[5],
           snapshot_.romCode[6],
           snapshot_.romCode[7]);
}

uint32_t SensorManager::conversionTimeMsForResolution(uint8_t resolutionBits) {
  switch (resolutionBits) {
    case 9:
      return 94;
    case 10:
      return 188;
    case 11:
      return 375;
    case 12:
    default:
      return 750;
  }
}

bool SensorManager::discoverSensor(uint32_t nowMs) {
  DeviceAddress address = {};

  snapshot_.lastDiscoveryMs = nowMs;
  snapshot_.presenceDetected = oneWire_.reset() != 0;
  snapshot_.configuredAddressMatched = false;
  snapshot_.discoveredSensorCount =
      (uint8_t)min<uint8_t>(sensors_.getDeviceCount(), 255);

  if (!snapshot_.presenceDetected || snapshot_.discoveredSensorCount == 0) {
    snapshot_.sensorDiscovered = false;
    snapshot_.sampleValid = false;
    snapshot_.conversionPending = false;
    snapshot_.temperatureC = NAN;
    clearAddress();
    return false;
  }

  if (config_.hasPreferredAddress) {
    memcpy(address, config_.preferredAddress, sizeof(address));
    snapshot_.configuredAddressMatched = sensors_.isConnected(address);
  } else {
    snapshot_.configuredAddressMatched = sensors_.getAddress(address, 0);
  }

  if (!snapshot_.configuredAddressMatched) {
    snapshot_.sensorDiscovered = false;
    snapshot_.sampleValid = false;
    snapshot_.conversionPending = false;
    snapshot_.temperatureC = NAN;
    clearAddress();
    return false;
  }

  memcpy(snapshot_.romCode, address, sizeof(snapshot_.romCode));
  snapshot_.sensorDiscovered = true;
  snapshot_.addressKnown = true;
  sensors_.setResolution(address, config_.resolutionBits);
  return true;
}

void SensorManager::startConversion(uint32_t nowMs) {
  if (!snapshot_.addressKnown) {
    return;
  }

  snapshot_.presenceDetected = oneWire_.reset() != 0;
  if (!snapshot_.presenceDetected) {
    snapshot_.sensorDiscovered = false;
    snapshot_.discoveredSensorCount = 0;
    snapshot_.sampleValid = false;
    snapshot_.temperatureC = NAN;
    snapshot_.lastDiscoveryMs = 0;
    return;
  }

  sensors_.requestTemperaturesByAddress(snapshot_.romCode);
  snapshot_.conversionPending = true;
  snapshot_.lastRequestMs = nowMs;
  conversionReadyAtMs_ = nowMs + conversionTimeMsForResolution(config_.resolutionBits);
}

void SensorManager::finishConversion(uint32_t nowMs) {
  snapshot_.conversionPending = false;

  if (!snapshot_.addressKnown) {
    snapshot_.sampleValid = false;
    snapshot_.temperatureC = NAN;
    return;
  }

  const float temperatureC = sensors_.getTempC(snapshot_.romCode);
  if (temperatureC == DEVICE_DISCONNECTED_C || isnan(temperatureC)) {
    snapshot_.sensorDiscovered = false;
    snapshot_.discoveredSensorCount = 0;
    snapshot_.sampleValid = false;
    snapshot_.temperatureC = NAN;
    snapshot_.lastDiscoveryMs = 0;
    return;
  }

  snapshot_.sampleValid = true;
  snapshot_.temperatureC = temperatureC;
  snapshot_.lastSampleMs = nowMs;
}

void SensorManager::clearAddress() {
  memset(snapshot_.romCode, 0, sizeof(snapshot_.romCode));
  snapshot_.addressKnown = false;
}
