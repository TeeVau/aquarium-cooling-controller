/**
 * @file sensor_manager.cpp
 * @brief Implements OneWire sensor discovery and non-blocking temperature sampling.
 */

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
  for (size_t index = 0; index < kMaxTrackedSensors; ++index) {
    clearTrackedSensor(index);
  }
  clearDiscoveredSensors();
}

bool SensorManager::begin(uint32_t nowMs) {
  sensors_.begin();
  sensors_.setWaitForConversion(false);
  sensors_.setCheckForConversion(false);
  sensors_.setResolution(config_.resolutionBits);

  initialized_ = true;
  snapshot_.busInitialized = true;

  if (discoverSensors(nowMs)) {
    startConversion(nowMs);
  }

  return initialized_;
}

void SensorManager::update(uint32_t nowMs) {
  if (!initialized_) {
    return;
  }

  const bool anyTrackedSensorKnown =
      snapshot_.trackedSensors[0].addressKnown ||
      snapshot_.trackedSensors[1].addressKnown;

  if (!anyTrackedSensorKnown) {
    if (snapshot_.lastDiscoveryMs == 0 ||
        nowMs - snapshot_.lastDiscoveryMs >= config_.sampleIntervalMs) {
      if (discoverSensors(nowMs)) {
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

void SensorManager::formatTrackedAddress(size_t trackedIndex,
                                         char* buffer,
                                         size_t bufferSize) const {
  if (bufferSize == 0) {
    return;
  }

  if (trackedIndex >= config_.trackedSensorCount ||
      !snapshot_.trackedSensors[trackedIndex].addressKnown ||
      bufferSize < (kRomCodeLength * 2U + 1U)) {
    snprintf(buffer, bufferSize, "n/a");
    return;
  }

  const uint8_t* romCode = snapshot_.trackedSensors[trackedIndex].romCode;
  snprintf(buffer, bufferSize,
           "%02X%02X%02X%02X%02X%02X%02X%02X",
           romCode[0],
           romCode[1],
           romCode[2],
           romCode[3],
           romCode[4],
           romCode[5],
           romCode[6],
           romCode[7]);
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

bool SensorManager::discoverSensors(uint32_t nowMs) {
  snapshot_.lastDiscoveryMs = nowMs;
  snapshot_.presenceDetected = oneWire_.reset() != 0;
  snapshot_.discoveredSensorCount =
      (uint8_t)min<uint8_t>(sensors_.getDeviceCount(), kMaxDiscoveredSensors);

  clearDiscoveredSensors();
  for (size_t trackedIndex = 0; trackedIndex < kMaxTrackedSensors; ++trackedIndex) {
    clearTrackedSensor(trackedIndex);
  }

  if (!snapshot_.presenceDetected || snapshot_.discoveredSensorCount == 0) {
    snapshot_.conversionPending = false;
    return false;
  }

  DeviceAddress discoveredAddress = {};
  for (uint8_t discoveredIndex = 0;
       discoveredIndex < snapshot_.discoveredSensorCount; ++discoveredIndex) {
    if (!sensors_.getAddress(discoveredAddress, discoveredIndex)) {
      continue;
    }

    DiscoveredSensorSnapshot& discovered =
        snapshot_.discoveredSensors[discoveredIndex];
    discovered.present = true;
    discovered.assigned = false;
    discovered.externallyPowered = sensors_.readPowerSupply(discoveredAddress);
    memcpy(discovered.romCode, discoveredAddress, sizeof(discovered.romCode));
    sensors_.setResolution(discoveredAddress, config_.resolutionBits);
  }

  for (size_t trackedIndex = 0; trackedIndex < config_.trackedSensorCount;
       ++trackedIndex) {
    const TrackedSensorConfig& trackedConfig = config_.trackedSensors[trackedIndex];
    TrackedSensorSnapshot& tracked = snapshot_.trackedSensors[trackedIndex];
    int matchedDiscoveredIndex = -1;

    if (trackedConfig.hasPreferredAddress) {
      matchedDiscoveredIndex = findDiscoveredIndexByAddress(trackedConfig.preferredAddress);
      tracked.configuredAddressMatched = matchedDiscoveredIndex >= 0;
      if (matchedDiscoveredIndex >= 0) {
        memcpy(tracked.romCode,
               snapshot_.discoveredSensors[matchedDiscoveredIndex].romCode,
               sizeof(tracked.romCode));
        tracked.externallyPowered =
            snapshot_.discoveredSensors[matchedDiscoveredIndex].externallyPowered;
      }
    } else {
      for (uint8_t discoveredIndex = 0;
           discoveredIndex < snapshot_.discoveredSensorCount; ++discoveredIndex) {
        if (!snapshot_.discoveredSensors[discoveredIndex].present ||
            snapshot_.discoveredSensors[discoveredIndex].assigned) {
          continue;
        }

        matchedDiscoveredIndex = discoveredIndex;
        tracked.configuredAddressMatched = true;
        memcpy(tracked.romCode,
               snapshot_.discoveredSensors[matchedDiscoveredIndex].romCode,
               sizeof(tracked.romCode));
        tracked.externallyPowered =
            snapshot_.discoveredSensors[matchedDiscoveredIndex].externallyPowered;
        break;
      }
    }

    if (matchedDiscoveredIndex < 0) {
      continue;
    }

    snapshot_.discoveredSensors[matchedDiscoveredIndex].assigned = true;
    tracked.addressKnown = true;
  }

  for (size_t trackedIndex = 0; trackedIndex < config_.trackedSensorCount;
       ++trackedIndex) {
    if (snapshot_.trackedSensors[trackedIndex].addressKnown) {
      return true;
    }
  }

  snapshot_.conversionPending = false;
  return false;
}

void SensorManager::startConversion(uint32_t nowMs) {
  snapshot_.presenceDetected = oneWire_.reset() != 0;
  if (!snapshot_.presenceDetected) {
    snapshot_.discoveredSensorCount = 0;
    clearDiscoveredSensors();
    for (size_t trackedIndex = 0; trackedIndex < kMaxTrackedSensors; ++trackedIndex) {
      clearTrackedSensor(trackedIndex);
    }
    snapshot_.lastDiscoveryMs = 0;
    snapshot_.conversionPending = false;
    return;
  }

  sensors_.requestTemperatures();
  snapshot_.conversionPending = true;
  snapshot_.lastRequestMs = nowMs;
  conversionReadyAtMs_ = nowMs + conversionTimeMsForResolution(config_.resolutionBits);
}

void SensorManager::finishConversion(uint32_t nowMs) {
  snapshot_.conversionPending = false;

  for (size_t trackedIndex = 0; trackedIndex < config_.trackedSensorCount;
       ++trackedIndex) {
    TrackedSensorSnapshot& tracked = snapshot_.trackedSensors[trackedIndex];
    if (!tracked.addressKnown) {
      tracked.sampleValid = false;
      tracked.temperatureC = NAN;
      continue;
    }

    const float temperatureC = sensors_.getTempC(tracked.romCode);
    if (temperatureC == DEVICE_DISCONNECTED_C || isnan(temperatureC)) {
      tracked.sampleValid = false;
      tracked.temperatureC = NAN;
      snapshot_.lastDiscoveryMs = 0;
      continue;
    }

    tracked.sampleValid = true;
    tracked.temperatureC = temperatureC;
    tracked.lastSampleMs = nowMs;
  }
}

void SensorManager::clearTrackedSensor(size_t trackedIndex) {
  TrackedSensorSnapshot& tracked = snapshot_.trackedSensors[trackedIndex];
  tracked.configuredAddressMatched = false;
  tracked.addressKnown = false;
  tracked.sampleValid = false;
  tracked.externallyPowered = false;
  tracked.temperatureC = NAN;
  tracked.lastSampleMs = 0;
  memset(tracked.romCode, 0, sizeof(tracked.romCode));
}

void SensorManager::clearDiscoveredSensors() {
  for (size_t discoveredIndex = 0; discoveredIndex < kMaxDiscoveredSensors;
       ++discoveredIndex) {
    DiscoveredSensorSnapshot& discovered =
        snapshot_.discoveredSensors[discoveredIndex];
    discovered.present = false;
    discovered.assigned = false;
    discovered.externallyPowered = false;
    memset(discovered.romCode, 0, sizeof(discovered.romCode));
  }
}

int SensorManager::findDiscoveredIndexByAddress(const uint8_t address[8]) const {
  for (uint8_t discoveredIndex = 0; discoveredIndex < snapshot_.discoveredSensorCount;
       ++discoveredIndex) {
    if (!snapshot_.discoveredSensors[discoveredIndex].present) {
      continue;
    }

    if (memcmp(snapshot_.discoveredSensors[discoveredIndex].romCode,
               address,
               kRomCodeLength) == 0) {
      return (int)discoveredIndex;
    }
  }

  return -1;
}
