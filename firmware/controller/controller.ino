/**
 * @file controller.ino
 * @brief Arduino sketch entry point for the aquarium cooling controller.
 *
 * The sketch wires the firmware modules together and owns the runtime loop. It
 * sets up persistent temperature-control storage, water-temperature
 * sensing, fan PWM output, tachometer RPM monitoring, fault evaluation, serial
 * diagnostics, and optional MQTT telemetry.
 *
 * Runtime behavior is locally autonomous by design: cooling control, fan safety
 * monitoring, and diagnostics continue on the ESP32 even when Wi-Fi, MQTT, or
 * external integrations are unavailable.
 *
 * @section controller_circuit Circuit
 *
 * - DS18B20 water probe on the shared OneWire bus.
 * - Four-wire PWM fan controlled by ESP32 LEDC output.
 * - Fan tachometer connected to an interrupt-capable ESP32 GPIO.
 *
 * @section controller_libraries Libraries
 *
 * - Arduino core for ESP32.
 * - Preferences for persisted target-temperature storage.
 * - OneWire and DallasTemperature through SensorManager.
 * - PubSubClient and WiFi through MqttTelemetry.
 *
 * @section controller_loop_order Main Loop Order
 *
 * - process serial commands,
 * - update sensor conversions,
 * - compute the fan PWM command,
 * - apply PWM and update start boost,
 * - update RPM sampling and MQTT connection state,
 * - periodically evaluate faults, print diagnostics, and publish telemetry.
 *
 * @section controller_serial Serial Commands
 *
 * The serial monitor exposes service commands for status output,
 * target-temperature changes, default reset, control defaults, fault-policy
 * settings, network status, and forced telemetry publishing.
 *
 * @section controller_notes Notes
 *
 * Keep this sketch focused on orchestration. Hardware abstractions and policy
 * decisions live in dedicated modules so Doxygen pages remain navigable and the
 * Arduino loop stays easy to audit.
 */

#include <Arduino.h>

#include <esp_arduino_version.h>
#include <esp_ota_ops.h>
#include <Preferences.h>

#include "control_engine.h"
#include "display_format.h"
#include "fan_driver.h"
#include "fan_curve.h"
#include "fault_monitor.h"
#include "fault_policy.h"
#include "mqtt_telemetry.h"
#include "oled_status_display.h"
#include "ota_upload_server.h"
#include "rpm_monitor.h"
#include "sensor_manager.h"

namespace {

#define AQ_FIRMWARE_VERSION "0.3.0"

constexpr char kFirmwareName[] = "aq-cooling-controller";
constexpr char kFirmwareVersion[] = AQ_FIRMWARE_VERSION;
constexpr char kFirmwareIdentityTag[] = "AQFW_PRODUCT=aq-cooling-controller";
constexpr char kFirmwareVersionTag[] = "AQFW_VERSION=" AQ_FIRMWARE_VERSION;
constexpr char kUnavailableText[] = "unavailable";
constexpr uint32_t kDiagnosticsIntervalMs = 2000;
constexpr size_t kSerialCommandBufferSize = 32;
constexpr size_t kSensorAddressBufferSize = 17;
constexpr size_t kWaterSensorIndex = 0;
constexpr size_t kNetworkIpBufferSize = 16;
constexpr size_t kOtaUploadUrlBufferSize = 40;
constexpr char kPreferencesNamespace[] = "controller";
constexpr char kKeyHasCustomTarget[] = "target_set";
constexpr char kKeyTargetTemperature[] = "target_c";
constexpr char kKeyCoolingOnDelta[] = "cool_on";
constexpr char kKeyCoolingOffDelta[] = "cool_off";
constexpr char kKeyHighCoolingDelta[] = "cool_high";
constexpr char kKeyFanLowPwm[] = "fan_low";
constexpr char kKeyFanHighPwm[] = "fan_high";
constexpr char kSetTargetTemperatureSuffix[] = "/set/target_temp_c";
constexpr char kSetCoolingOnDeltaSuffix[] = "/set/cooling_on_delta_c";
constexpr char kSetCoolingOffDeltaSuffix[] = "/set/cooling_off_delta_c";
constexpr char kSetHighCoolingDeltaSuffix[] = "/set/high_cooling_delta_c";
constexpr char kSetFanLowPwmSuffix[] = "/set/fan_low_pwm_percent";
constexpr char kSetFanHighPwmSuffix[] = "/set/fan_high_pwm_percent";
constexpr char kSetOtaEnableSuffix[] = "/set/ota_enable";
constexpr size_t kRemoteConfigPayloadBufferSize = 32;

constexpr uint8_t kWaterSensorRomCode[8] = {
    0x28, 0x33, 0x38, 0x44, 0x05, 0x00, 0x00, 0xCB,
};

constexpr SensorManagerConfig kSensorManagerConfig = {
    33,
    2000,
    12,
    1,
    {
        {
            true,
            {0x28, 0x33, 0x38, 0x44, 0x05, 0x00, 0x00, 0xCB},
            "Water sensor",
        },
    },
};

constexpr OledStatusDisplayConfig kOledStatusDisplayConfig = {
    32,
    27,
    128,
    32,
    0x3C,
    0x3D,
    400000,
};

FanDriver fanDriver;
RpmMonitor rpmMonitor;
FaultMonitor faultMonitor;
SensorManager sensorManager(kSensorManagerConfig);
MqttTelemetry mqttTelemetry;
OledStatusDisplay oledStatusDisplay(kOledStatusDisplayConfig);
OtaUploadServer otaUploadServer;
Preferences preferences;
ControlConfig runtimeControlConfig = kDefaultControlConfig;
ControlSnapshot lastControlSnapshot = {};
FaultMonitorSnapshot lastFaultSnapshot = {};
FaultPolicySnapshot lastFaultPolicySnapshot = {};
OtaTelemetrySnapshot otaTelemetrySnapshot = {
    false,
    "disabled",
    "OTA upload disabled.",
    kFirmwareVersion,
    kUnavailableText,
    kUnavailableText,
};
RemoteConfigStatus remoteConfigStatus = {};
uint32_t lastDiagnosticsMs = 0;
float requestedTargetTemperatureC = kDefaultControlConfig.defaultTargetTemperatureC;
bool hasConfiguredTargetTemperature = false;
bool preferencesReady = false;
bool lastFaultSnapshotValid = false;
bool telemetryPublishRequested = false;
char serialCommandBuffer[kSerialCommandBufferSize] = {};
size_t serialCommandLength = 0;
char mqttNetworkIpBuffer[kNetworkIpBufferSize] = "unavailable";
char mqttOtaUploadUrlBuffer[kOtaUploadUrlBufferSize] = "unavailable";
struct TelemetryEventState {
  ControlMode controlMode;
  uint8_t fanPwmPercent;
  bool fanFault;
  AlarmCode alarmCode;
  FaultSeverity faultSeverity;
  FaultResponse faultResponse;
  bool waterSensorOk;
  bool coolingDegraded;
  bool serviceRequired;
};
TelemetryEventState lastPublishedTelemetryEventState = {};
bool lastPublishedTelemetryEventStateValid = false;

TelemetryEventState buildTelemetryEventState(const ControlSnapshot& controlSnapshot,
                                             const FaultMonitorSnapshot& faultSnapshot,
                                             const FaultPolicySnapshot& policySnapshot) {
  return {
      controlSnapshot.mode,
      controlSnapshot.finalPwmPercent,
      faultSnapshot.faultLatched,
      policySnapshot.alarmCode,
      policySnapshot.severity,
      policySnapshot.response,
      policySnapshot.waterSensorOk,
      policySnapshot.coolingDegraded,
      policySnapshot.serviceRequired,
  };
}

bool telemetryEventStateEquals(const TelemetryEventState& lhs,
                               const TelemetryEventState& rhs) {
  return lhs.controlMode == rhs.controlMode &&
         lhs.fanPwmPercent == rhs.fanPwmPercent && lhs.fanFault == rhs.fanFault &&
         lhs.alarmCode == rhs.alarmCode &&
         lhs.faultSeverity == rhs.faultSeverity &&
         lhs.faultResponse == rhs.faultResponse &&
         lhs.waterSensorOk == rhs.waterSensorOk &&
         lhs.coolingDegraded == rhs.coolingDegraded &&
         lhs.serviceRequired == rhs.serviceRequired;
}

void printHelp() {
  Serial.println("Serial commands:");
  Serial.println("  status        -> print diagnostics immediately");
  Serial.println("  target <c>    -> set target water temperature in C");
  Serial.println("  default       -> reset target temperature to default 23.0 C");
  Serial.println("  control       -> print current hysteresis and quiet-cooling defaults");
  Serial.println("  faults        -> print current fault-policy defaults");
  Serial.println("  network       -> print Wi-Fi/MQTT telemetry status");
  Serial.println("  publish       -> publish telemetry immediately when MQTT is connected");
  Serial.println("  ota status    -> print OTA upload status");
  Serial.println("  ota enable    -> open temporary OTA .bin upload window");
  Serial.println("  ota cancel    -> close OTA upload window");
  Serial.println("  help          -> show this help");
  Serial.println();
}

void printCoreVersion() {
  Serial.print("Firmware: ");
  Serial.print(kFirmwareName);
  Serial.print(" ");
  Serial.println(kFirmwareVersion);

  Serial.print("ESP32 Arduino core: ");
  Serial.print(ESP_ARDUINO_VERSION_MAJOR);
  Serial.print(".");
  Serial.print(ESP_ARDUINO_VERSION_MINOR);
  Serial.print(".");
  Serial.println(ESP_ARDUINO_VERSION_PATCH);
}

void printConfiguredRom(const char* label, const uint8_t romCode[8]) {
  Serial.print(label);
  for (size_t i = 0; i < 8; ++i) {
    if (romCode[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(romCode[i], HEX);
  }
  Serial.println();
}

void printCurveSummary() {
  Serial.println("Measured production fan curve:");

  const FanCurvePoint* curve = FanCurve::points();
  for (size_t i = 0; i < FanCurve::pointCount(); ++i) {
    Serial.print("  ");
    Serial.print(curve[i].pwmPercent);
    Serial.print("% -> ");
    Serial.print(curve[i].rpm);
    Serial.println(" RPM");
  }

  Serial.println();
  Serial.print("Start PWM: ");
  Serial.print(FanCurve::kStartPwmPercent);
  Serial.println("%");

  Serial.print("Minimum hold PWM: ");
  Serial.print(FanCurve::kMinimumHoldPwmPercent);
  Serial.println("%");

  Serial.print("Startup plausibility tolerance: +/-");
  Serial.print(FanCurve::kPlausibilityTolerancePercent);
  Serial.println("%");
}

bool beginPreferences() {
  preferencesReady = preferences.begin(kPreferencesNamespace, false);
  return preferencesReady;
}

void syncOtaTelemetrySnapshot() {
  otaTelemetrySnapshot.active = otaUploadServer.active();
  otaTelemetrySnapshot.stateLabel = otaUploadServer.statusLabel();
  otaTelemetrySnapshot.lastMessage = otaUploadServer.lastMessage();
  otaTelemetrySnapshot.firmwareVersion = kFirmwareVersion;

  if (WiFi.status() == WL_CONNECTED) {
    const IPAddress wifiIp = WiFi.localIP();
    snprintf(mqttNetworkIpBuffer,
             sizeof(mqttNetworkIpBuffer),
             "%u.%u.%u.%u",
             wifiIp[0],
             wifiIp[1],
             wifiIp[2],
             wifiIp[3]);
  } else {
    snprintf(mqttNetworkIpBuffer, sizeof(mqttNetworkIpBuffer), "%s", kUnavailableText);
  }

  otaTelemetrySnapshot.networkIp = mqttNetworkIpBuffer;

  if (otaUploadServer.active() && WiFi.status() == WL_CONNECTED) {
    snprintf(mqttOtaUploadUrlBuffer,
             sizeof(mqttOtaUploadUrlBuffer),
             "http://%s/update",
             mqttNetworkIpBuffer);
  } else {
    snprintf(mqttOtaUploadUrlBuffer,
             sizeof(mqttOtaUploadUrlBuffer),
             "%s",
             kUnavailableText);
  }

  otaTelemetrySnapshot.uploadUrl = mqttOtaUploadUrlBuffer;
}

void requestTelemetryPublish() {
  telemetryPublishRequested = true;
}

void markTelemetryPublishSuccessful(const ControlSnapshot& controlSnapshot,
                                    const FaultMonitorSnapshot& faultSnapshot,
                                    const FaultPolicySnapshot& policySnapshot) {
  lastPublishedTelemetryEventState =
      buildTelemetryEventState(controlSnapshot, faultSnapshot, policySnapshot);
  lastPublishedTelemetryEventStateValid = true;
  telemetryPublishRequested = false;
}

void setRemoteConfigStatus(bool accepted, const char* key, const char* detail) {
  remoteConfigStatus.lastCommandSeen = true;
  remoteConfigStatus.lastCommandAccepted = accepted;

  if (accepted) {
    ++remoteConfigStatus.acceptedCount;
  } else {
    ++remoteConfigStatus.rejectedCount;
  }

  snprintf(remoteConfigStatus.lastKey,
           sizeof(remoteConfigStatus.lastKey),
           "%s",
           key != nullptr ? key : "unknown");
  snprintf(remoteConfigStatus.lastDetail,
           sizeof(remoteConfigStatus.lastDetail),
           "%s",
           detail != nullptr ? detail : "none");
  requestTelemetryPublish();
}

void clearPersistedTargetTemperature() {
  if (!preferencesReady) {
    return;
  }

  preferences.remove(kKeyHasCustomTarget);
  preferences.remove(kKeyTargetTemperature);
}

void clearPersistedControlConfig() {
  if (!preferencesReady) {
    return;
  }

  preferences.remove(kKeyCoolingOnDelta);
  preferences.remove(kKeyCoolingOffDelta);
  preferences.remove(kKeyHighCoolingDelta);
  preferences.remove(kKeyFanLowPwm);
  preferences.remove(kKeyFanHighPwm);
}

void persistTargetTemperature(float targetTemperatureC) {
  if (!preferencesReady) {
    return;
  }

  preferences.putBool(kKeyHasCustomTarget, true);
  preferences.putFloat(kKeyTargetTemperature, targetTemperatureC);
}

void persistControlConfig(const ControlConfig& config) {
  if (!preferencesReady) {
    return;
  }

  preferences.putFloat(kKeyCoolingOnDelta, config.coolingOnDeltaC);
  preferences.putFloat(kKeyCoolingOffDelta, config.coolingOffDeltaC);
  preferences.putFloat(kKeyHighCoolingDelta, config.highCoolingDeltaC);
  preferences.putUChar(kKeyFanLowPwm, config.fanLowPwmPercent);
  preferences.putUChar(kKeyFanHighPwm, config.fanHighPwmPercent);
}

const char* validateControlConfigDetail(const ControlConfig& config) {
  if (!isfinite(config.coolingOnDeltaC) ||
      config.coolingOnDeltaC < config.minimumCoolingOnDeltaC ||
      config.coolingOnDeltaC > config.maximumCoolingOnDeltaC ||
      config.coolingOnDeltaC <= 0.0f) {
    return "cooling_on_delta_c out of allowed range";
  }

  if (!isfinite(config.coolingOffDeltaC) ||
      config.coolingOffDeltaC < config.minimumCoolingOffDeltaC ||
      config.coolingOffDeltaC > config.maximumCoolingOffDeltaC ||
      config.coolingOffDeltaC >= 0.0f) {
    return "cooling_off_delta_c out of allowed range";
  }

  if (!isfinite(config.highCoolingDeltaC) ||
      config.highCoolingDeltaC < config.minimumHighCoolingDeltaC ||
      config.highCoolingDeltaC > config.maximumHighCoolingDeltaC) {
    return "high_cooling_delta_c out of allowed range";
  }

  if (config.highCoolingDeltaC <= config.coolingOnDeltaC ||
      (config.highCoolingDeltaC - config.coolingOnDeltaC) < 0.1f) {
    return "high_cooling_delta_c must exceed cooling_on_delta_c";
  }

  if (config.fanLowPwmPercent < config.minimumFanLowPwmPercent ||
      config.fanLowPwmPercent > config.maximumFanLowPwmPercent) {
    return "fan_low_pwm_percent out of allowed range";
  }

  if (config.fanHighPwmPercent < config.minimumFanHighPwmPercent ||
      config.fanHighPwmPercent > config.maximumFanHighPwmPercent) {
    return "fan_high_pwm_percent out of allowed range";
  }

  if (config.fanHighPwmPercent <= config.fanLowPwmPercent) {
    return "fan_high_pwm_percent must exceed fan_low_pwm_percent";
  }

  return nullptr;
}

bool applyRuntimeControlConfig(const ControlConfig& candidate, const char** detailOut) {
  if (!ControlEngine::isControlConfigValid(candidate)) {
    if (detailOut != nullptr) {
      *detailOut = validateControlConfigDetail(candidate);
    }
    return false;
  }

  runtimeControlConfig = candidate;
  persistControlConfig(runtimeControlConfig);
  requestTelemetryPublish();

  if (detailOut != nullptr) {
    *detailOut = "persisted";
  }
  return true;
}

void loadPersistedTargetTemperature() {
  requestedTargetTemperatureC = kDefaultControlConfig.defaultTargetTemperatureC;
  hasConfiguredTargetTemperature = false;

  if (!preferencesReady) {
    return;
  }

  const bool hasStoredTarget = preferences.getBool(kKeyHasCustomTarget, false);
  const float storedTargetTemperatureC =
      preferences.getFloat(kKeyTargetTemperature,
                           kDefaultControlConfig.defaultTargetTemperatureC);

  if (hasStoredTarget &&
      ControlEngine::isTargetTemperatureValid(storedTargetTemperatureC)) {
    requestedTargetTemperatureC = storedTargetTemperatureC;
    hasConfiguredTargetTemperature = true;
    return;
  }

  if (hasStoredTarget) {
    clearPersistedTargetTemperature();
  }
}

void initializeRuntimeControlConfig() {
  runtimeControlConfig = kDefaultControlConfig;

  if (!preferencesReady) {
    return;
  }

  ControlConfig candidate = runtimeControlConfig;
  candidate.coolingOnDeltaC =
      preferences.getFloat(kKeyCoolingOnDelta, kDefaultControlConfig.coolingOnDeltaC);
  candidate.coolingOffDeltaC =
      preferences.getFloat(kKeyCoolingOffDelta, kDefaultControlConfig.coolingOffDeltaC);
  candidate.highCoolingDeltaC =
      preferences.getFloat(kKeyHighCoolingDelta, kDefaultControlConfig.highCoolingDeltaC);
  candidate.fanLowPwmPercent =
      preferences.getUChar(kKeyFanLowPwm, kDefaultControlConfig.fanLowPwmPercent);
  candidate.fanHighPwmPercent =
      preferences.getUChar(kKeyFanHighPwm, kDefaultControlConfig.fanHighPwmPercent);

  if (!ControlEngine::isControlConfigValid(candidate)) {
    clearPersistedControlConfig();
    return;
  }

  runtimeControlConfig = candidate;
}

void printTemperatureLine(const char* label, float value) {
  Serial.print(label);
  DisplayFormat::printTemperatureC(Serial, value);
  Serial.println(" C");
}

void printTrackedSensorDetails(const SensorSnapshot& sensorSnapshot, uint32_t nowMs) {
  for (size_t trackedIndex = 0;
       trackedIndex < kSensorManagerConfig.trackedSensorCount;
       ++trackedIndex) {
    const TrackedSensorSnapshot& tracked =
        sensorSnapshot.trackedSensors[trackedIndex];
    char sensorAddress[kSensorAddressBufferSize] = {};
    sensorManager.formatTrackedAddress(trackedIndex, sensorAddress, sizeof(sensorAddress));

    Serial.print("  ");
    Serial.print(kSensorManagerConfig.trackedSensors[trackedIndex].sensorLabel);
    Serial.print(" matched: ");
    Serial.println(tracked.configuredAddressMatched ? "yes" : "no");

    Serial.print("  ");
    Serial.print(kSensorManagerConfig.trackedSensors[trackedIndex].sensorLabel);
    Serial.print(" ROM: ");
    Serial.println(sensorAddress);

    Serial.print("  ");
    Serial.print(kSensorManagerConfig.trackedSensors[trackedIndex].sensorLabel);
    Serial.print(" sample valid: ");
    Serial.println(tracked.sampleValid ? "yes" : "no");

    Serial.print("  ");
    Serial.print(kSensorManagerConfig.trackedSensors[trackedIndex].sensorLabel);
    Serial.print(" power mode: ");
    Serial.println(tracked.externallyPowered ? "external" : "parasite/unknown");

    Serial.print("  ");
    Serial.print(kSensorManagerConfig.trackedSensors[trackedIndex].sensorLabel);
    Serial.print(" temperature: ");
    if (tracked.sampleValid) {
      DisplayFormat::printTemperatureC(Serial, tracked.temperatureC);
      Serial.println(" C");
    } else {
      Serial.println("unavailable");
    }

    Serial.print("  ");
    Serial.print(kSensorManagerConfig.trackedSensors[trackedIndex].sensorLabel);
    Serial.print(" sample age: ");
    if (tracked.sampleValid) {
      Serial.print(nowMs - tracked.lastSampleMs);
      Serial.println(" ms");
    } else {
      Serial.println("n/a");
    }
  }
}

void printDiscoveredBusSensors(const SensorSnapshot& sensorSnapshot) {
  for (uint8_t discoveredIndex = 0;
       discoveredIndex < sensorSnapshot.discoveredSensorCount &&
       discoveredIndex < kMaxDiscoveredSensors;
       ++discoveredIndex) {
    const DiscoveredSensorSnapshot& discovered =
        sensorSnapshot.discoveredSensors[discoveredIndex];
    if (!discovered.present) {
      continue;
    }

    char sensorAddress[kSensorAddressBufferSize] = {};
    snprintf(sensorAddress,
             sizeof(sensorAddress),
             "%02X%02X%02X%02X%02X%02X%02X%02X",
             discovered.romCode[0],
             discovered.romCode[1],
             discovered.romCode[2],
             discovered.romCode[3],
             discovered.romCode[4],
             discovered.romCode[5],
             discovered.romCode[6],
             discovered.romCode[7]);

    Serial.print("  Bus sensor ");
    Serial.print(discoveredIndex);
    Serial.print(": ");
    Serial.print(sensorAddress);
    Serial.print(" (");
    Serial.print(discovered.assigned ? "assigned" : "unassigned");
    Serial.print(", ");
    Serial.print(discovered.externallyPowered ? "external" : "parasite/unknown");
    Serial.println(")");
  }
}

void printControlDetails() {
  Serial.print("  Control mode: ");
  Serial.println(ControlEngine::modeLabel(lastControlSnapshot.mode));

  printTemperatureLine("  Target temperature: ", lastControlSnapshot.targetTemperatureC);

  Serial.print("  Target source: ");
  Serial.println(hasConfiguredTargetTemperature ? "persisted/custom" : "default");

  Serial.print("  Target defaulted: ");
  Serial.println(lastControlSnapshot.targetDefaulted ? "yes" : "no");

  Serial.print("  Water delta: ");
  if (lastControlSnapshot.waterSensorValid && isfinite(lastControlSnapshot.waterDeltaC)) {
    DisplayFormat::printTemperatureC(Serial, lastControlSnapshot.waterDeltaC);
    Serial.println(" C");
  } else {
    Serial.println("n/a");
  }

  Serial.print("  Water-based PWM: ");
  Serial.print(lastControlSnapshot.waterBasedPwmPercent);
  Serial.println("%");

  Serial.print("  Cooling enters at target delta: +");
  DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOnDeltaC);
  Serial.println(" C");

  Serial.print("  Cooling enters fan-high at target delta: +");
  DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.highCoolingDeltaC);
  Serial.println(" C");

  Serial.print("  Cooling leaves at target delta: ");
  DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOffDeltaC);
  Serial.println(" C");

  Serial.print("  Fan-low PWM: ");
  Serial.print(runtimeControlConfig.fanLowPwmPercent);
  Serial.println("%");

  Serial.print("  Fan-high PWM: ");
  Serial.print(runtimeControlConfig.fanHighPwmPercent);
  Serial.println("%");

  Serial.print("  Final target PWM: ");
  Serial.print(lastControlSnapshot.finalPwmPercent);
  Serial.println("%");
}

void printFaultPolicyDefaults() {
  Serial.println("Fault policy:");
  Serial.print("  Water sensor fault response: ");
  Serial.print(FaultPolicy::responseLabel(FaultResponse::kWaterFallback));
  Serial.print(" at ");
  Serial.print(kDefaultControlConfig.fallbackPwmPercent);
  Serial.println("% PWM");

  Serial.print("  Fan fault response: ");
  Serial.println(FaultPolicy::responseLabel(FaultResponse::kReportFanFault));

  Serial.print("  Fan fault latch after mismatches: ");
  Serial.println(kDefaultFaultMonitorConfig.mismatchCyclesRequired);

  Serial.print("  Fan fault recovery after matches: ");
  Serial.println(kDefaultFaultMonitorConfig.matchCyclesRequiredForRecovery);

  Serial.print("  Fan settling time: ");
  Serial.print(kDefaultFaultMonitorConfig.settlingTimeMs);
  Serial.println(" ms");
}

void printRemoteConfigStatus() {
  Serial.print("  Remote config accepts: ");
  Serial.println(remoteConfigStatus.acceptedCount);

  Serial.print("  Remote config rejects: ");
  Serial.println(remoteConfigStatus.rejectedCount);

  Serial.print("  Remote config last result: ");
  if (!remoteConfigStatus.lastCommandSeen) {
    Serial.println("none");
    return;
  }

  Serial.println(remoteConfigStatus.lastCommandAccepted ? "accepted" : "rejected");
  Serial.print("  Remote config last key: ");
  Serial.println(remoteConfigStatus.lastKey);
  Serial.print("  Remote config last detail: ");
  Serial.println(remoteConfigStatus.lastDetail);
}

void printDisplayStatus() {
  Serial.print("  OLED available: ");
  Serial.println(oledStatusDisplay.isAvailable() ? "yes" : "no");

  Serial.print("  OLED mode: ");
  Serial.println(oledStatusDisplay.modeLabel());

  Serial.print("  OLED primary text: ");
  if (oledStatusDisplay.primaryText()[0] == '\0') {
    Serial.println("-");
  } else {
    Serial.println(oledStatusDisplay.primaryText());
  }

  Serial.print("  OLED secondary text: ");
  if (oledStatusDisplay.secondaryText()[0] == '\0') {
    Serial.println("-");
  } else {
    Serial.println(oledStatusDisplay.secondaryText());
  }

  if (oledStatusDisplay.isAvailable()) {
    Serial.print("  OLED I2C address: 0x");
    Serial.println(oledStatusDisplay.address(), HEX);
  }
}

void confirmRunningOtaImageIfNeeded() {
  const esp_partition_t* runningPartition = esp_ota_get_running_partition();
  if (runningPartition == nullptr) {
    Serial.println("OTA validation status: running partition unavailable.");
    return;
  }

  esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
  if (esp_ota_get_state_partition(runningPartition, &otaState) != ESP_OK) {
    Serial.println("OTA validation status: state check unavailable.");
    return;
  }

  if (otaState != ESP_OTA_IMG_PENDING_VERIFY) {
    return;
  }

  const esp_err_t markResult = esp_ota_mark_app_valid_cancel_rollback();
  if (markResult == ESP_OK) {
    Serial.println("OTA validation status: running image marked valid.");
  } else {
    Serial.print("OTA validation status: mark valid failed (");
    Serial.print((int)markResult);
    Serial.println(").");
  }
}

void printDiagnostics(const FaultMonitorSnapshot& snapshot,
                      const FaultPolicySnapshot& policySnapshot,
                      uint32_t sampleAgeMs,
                      uint32_t nowMs) {
  const SensorSnapshot& sensorSnapshot = sensorManager.snapshot();

  Serial.println("Controller diagnostics:");
  Serial.print("  Firmware: ");
  Serial.print(kFirmwareName);
  Serial.print(" ");
  Serial.println(kFirmwareVersion);

  printControlDetails();

  Serial.print("  Applied PWM: ");
  Serial.print(snapshot.commandedPwmPercent);
  Serial.println("%");

  Serial.print("  Measured RPM: ");
  Serial.println(snapshot.measuredRpm);

  Serial.print("  Expected RPM: ");
  Serial.println(snapshot.expectedRpm);

  Serial.print("  Tolerance RPM: +/-");
  Serial.println(snapshot.toleranceRpm);

  Serial.print("  RPM error: ");
  Serial.println(snapshot.rpmError);

  Serial.print("  Plausibility active: ");
  Serial.println(snapshot.plausibilityActive ? "yes" : "no");

  Serial.print("  Plausible: ");
  Serial.println(snapshot.plausible ? "yes" : "no");

  Serial.print("  Fault latched: ");
  Serial.println(snapshot.faultLatched ? "yes" : "no");

  Serial.print("  Alarm code: ");
  Serial.println(FaultPolicy::alarmCodeLabel(policySnapshot.alarmCode));

  Serial.print("  Fault severity: ");
  Serial.println(FaultPolicy::severityLabel(policySnapshot.severity));

  Serial.print("  Fault response: ");
  Serial.println(FaultPolicy::responseLabel(policySnapshot.response));

  Serial.print("  Cooling degraded: ");
  Serial.println(policySnapshot.coolingDegraded ? "yes" : "no");

  Serial.print("  Service required: ");
  Serial.println(policySnapshot.serviceRequired ? "yes" : "no");

  Serial.print("  Water sensor ok: ");
  Serial.println(policySnapshot.waterSensorOk ? "yes" : "no");

  Serial.print("  Fan ok: ");
  Serial.println(policySnapshot.fanOk ? "yes" : "no");

  printDisplayStatus();
  printRemoteConfigStatus();

  Serial.print("  Start boost active: ");
  Serial.println(fanDriver.isStartBoostActive() ? "yes" : "no");

  Serial.print("  RPM sample age: ");
  Serial.print(sampleAgeMs);
  Serial.println(" ms");

  Serial.print("  1-Wire bus pin: GPIO");
  Serial.println(kSensorManagerConfig.oneWirePin);

  Serial.print("  Sensors found: ");
  Serial.println(sensorSnapshot.discoveredSensorCount);

  Serial.print("  1-Wire presence pulse: ");
  Serial.println(sensorSnapshot.presenceDetected ? "yes" : "no");

  Serial.print("  Bus conversion pending: ");
  Serial.println(sensorSnapshot.conversionPending ? "yes" : "no");

  printTrackedSensorDetails(sensorSnapshot, nowMs);
  printDiscoveredBusSensors(sensorSnapshot);

  Serial.println();
}

void resetTargetTemperatureToDefault() {
  requestedTargetTemperatureC = kDefaultControlConfig.defaultTargetTemperatureC;
  hasConfiguredTargetTemperature = false;
  clearPersistedTargetTemperature();
  requestTelemetryPublish();
}

bool setTargetTemperature(float targetTemperatureC) {
  if (!ControlEngine::isTargetTemperatureValid(targetTemperatureC, runtimeControlConfig)) {
    return false;
  }

  requestedTargetTemperatureC = targetTemperatureC;
  hasConfiguredTargetTemperature = true;
  persistTargetTemperature(requestedTargetTemperatureC);
  requestTelemetryPublish();
  return true;
}

bool updateControlConfigFloat(float value,
                              float ControlConfig::*member,
                              const char** detailOut) {
  (void)detailOut;
  ControlConfig candidate = runtimeControlConfig;
  candidate.*member = value;
  return applyRuntimeControlConfig(candidate, detailOut);
}

bool updateControlConfigPwm(uint8_t value,
                            uint8_t ControlConfig::*member,
                            const char** detailOut) {
  ControlConfig candidate = runtimeControlConfig;
  candidate.*member = value;
  return applyRuntimeControlConfig(candidate, detailOut);
}

bool copyRemoteConfigPayload(const uint8_t* payload,
                             size_t length,
                             char* buffer,
                             size_t bufferSize) {
  if (payload == nullptr || buffer == nullptr || length >= bufferSize) {
    return false;
  }

  memcpy(buffer, payload, length);
  buffer[length] = '\0';
  return true;
}

bool parseBoolPayload(const char* text, bool* value) {
  if (text == nullptr || value == nullptr) {
    return false;
  }

  if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0 ||
      strcmp(text, "on") == 0) {
    *value = true;
    return true;
  }

  if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0 ||
      strcmp(text, "off") == 0) {
    *value = false;
    return true;
  }

  return false;
}

bool parseFloatPayload(const char* text, float* value) {
  if (text == nullptr || value == nullptr) {
    return false;
  }

  char* endPtr = nullptr;
  const float parsedValue = strtof(text, &endPtr);
  if (endPtr == nullptr || *endPtr != '\0' || !isfinite(parsedValue)) {
    return false;
  }

  *value = parsedValue;
  return true;
}

bool parseUInt8Payload(const char* text, uint8_t* value) {
  if (text == nullptr || value == nullptr || text[0] == '\0') {
    return false;
  }

  char* endPtr = nullptr;
  const unsigned long parsedValue = strtoul(text, &endPtr, 10);
  if (endPtr == nullptr || *endPtr != '\0' || parsedValue > 255UL) {
    return false;
  }

  *value = static_cast<uint8_t>(parsedValue);
  return true;
}

void handleRemoteConfigMessage(const char* suffix,
                               const uint8_t* payload,
                               size_t length,
                               void* context) {
  (void)context;

  char payloadText[kRemoteConfigPayloadBufferSize] = {};
  if (!copyRemoteConfigPayload(payload, length, payloadText, sizeof(payloadText))) {
    setRemoteConfigStatus(false, suffix, "payload too long");
    Serial.print("Remote config rejected for ");
    Serial.print(suffix);
    Serial.println(": payload too long.");
    return;
  }

  if (strcmp(suffix, kSetTargetTemperatureSuffix) == 0) {
    float parsedTargetTemperatureC = NAN;
    if (!parseFloatPayload(payloadText, &parsedTargetTemperatureC) ||
        !setTargetTemperature(parsedTargetTemperatureC)) {
      setRemoteConfigStatus(false,
                            "target_temp_c",
                            "target out of allowed range");
      Serial.print("Remote target rejected: ");
      Serial.println(payloadText);
      return;
    }

    setRemoteConfigStatus(true, "target_temp_c", "persisted");
    Serial.print("Remote target applied: ");
    DisplayFormat::printTemperatureC(Serial, requestedTargetTemperatureC);
    Serial.println(" C");
    return;
  }

  const char* detail = nullptr;
  if (strcmp(suffix, kSetCoolingOnDeltaSuffix) == 0) {
    float value = NAN;
    if (!parseFloatPayload(payloadText, &value) ||
        !updateControlConfigFloat(value,
                                  &ControlConfig::coolingOnDeltaC,
                                  &detail)) {
      setRemoteConfigStatus(false,
                            "cooling_on_delta_c",
                            detail != nullptr ? detail : "invalid value");
      Serial.print("Remote cooling_on_delta_c rejected: ");
      Serial.println(payloadText);
      return;
    }

    setRemoteConfigStatus(true, "cooling_on_delta_c", detail);
    Serial.print("Remote cooling_on_delta_c applied: ");
    DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOnDeltaC);
    Serial.println(" C");
    return;
  }

  if (strcmp(suffix, kSetCoolingOffDeltaSuffix) == 0) {
    float value = NAN;
    if (!parseFloatPayload(payloadText, &value) ||
        !updateControlConfigFloat(value,
                                  &ControlConfig::coolingOffDeltaC,
                                  &detail)) {
      setRemoteConfigStatus(false,
                            "cooling_off_delta_c",
                            detail != nullptr ? detail : "invalid value");
      Serial.print("Remote cooling_off_delta_c rejected: ");
      Serial.println(payloadText);
      return;
    }

    setRemoteConfigStatus(true, "cooling_off_delta_c", detail);
    Serial.print("Remote cooling_off_delta_c applied: ");
    DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOffDeltaC);
    Serial.println(" C");
    return;
  }

  if (strcmp(suffix, kSetHighCoolingDeltaSuffix) == 0) {
    float value = NAN;
    if (!parseFloatPayload(payloadText, &value) ||
        !updateControlConfigFloat(value,
                                  &ControlConfig::highCoolingDeltaC,
                                  &detail)) {
      setRemoteConfigStatus(false,
                            "high_cooling_delta_c",
                            detail != nullptr ? detail : "invalid value");
      Serial.print("Remote high_cooling_delta_c rejected: ");
      Serial.println(payloadText);
      return;
    }

    setRemoteConfigStatus(true, "high_cooling_delta_c", detail);
    Serial.print("Remote high_cooling_delta_c applied: ");
    DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.highCoolingDeltaC);
    Serial.println(" C");
    return;
  }

  if (strcmp(suffix, kSetFanLowPwmSuffix) == 0) {
    uint8_t value = 0;
    if (!parseUInt8Payload(payloadText, &value) ||
        !updateControlConfigPwm(value,
                                &ControlConfig::fanLowPwmPercent,
                                &detail)) {
      setRemoteConfigStatus(false,
                            "fan_low_pwm_percent",
                            detail != nullptr ? detail : "invalid value");
      Serial.print("Remote fan_low_pwm_percent rejected: ");
      Serial.println(payloadText);
      return;
    }

    setRemoteConfigStatus(true, "fan_low_pwm_percent", detail);
    Serial.print("Remote fan_low_pwm_percent applied: ");
    Serial.print(runtimeControlConfig.fanLowPwmPercent);
    Serial.println("%");
    return;
  }

  if (strcmp(suffix, kSetFanHighPwmSuffix) == 0) {
    uint8_t value = 0;
    if (!parseUInt8Payload(payloadText, &value) ||
        !updateControlConfigPwm(value,
                                &ControlConfig::fanHighPwmPercent,
                                &detail)) {
      setRemoteConfigStatus(false,
                            "fan_high_pwm_percent",
                            detail != nullptr ? detail : "invalid value");
      Serial.print("Remote fan_high_pwm_percent rejected: ");
      Serial.println(payloadText);
      return;
    }

    setRemoteConfigStatus(true, "fan_high_pwm_percent", detail);
    Serial.print("Remote fan_high_pwm_percent applied: ");
    Serial.print(runtimeControlConfig.fanHighPwmPercent);
    Serial.println("%");
    return;
  }

  if (strcmp(suffix, kSetOtaEnableSuffix) == 0) {
    bool enableRequested = false;
    if (!parseBoolPayload(payloadText, &enableRequested)) {
      setRemoteConfigStatus(false, "ota_enable", "expected true/false or 1/0");
      Serial.print("Remote OTA control rejected: ");
      Serial.println(payloadText);
      return;
    }

    if (enableRequested) {
      const bool alreadyActive = otaUploadServer.active();
      if (!otaUploadServer.enable(millis(), Serial)) {
        syncOtaTelemetrySnapshot();
        setRemoteConfigStatus(false, "ota_enable", otaUploadServer.lastMessage());
        return;
      }

      syncOtaTelemetrySnapshot();
      setRemoteConfigStatus(true,
                            "ota_enable",
                            alreadyActive ? "already active" : "ota window active");
      Serial.println(alreadyActive ? "Remote OTA window already active."
                                   : "Remote OTA window enabled.");
      return;
    }

    otaUploadServer.cancel(Serial);
    syncOtaTelemetrySnapshot();
    setRemoteConfigStatus(true, "ota_enable", "ota window cancelled");
    Serial.println("Remote OTA window cancelled.");
  }
}

void handleSerialCommand(const char* command) {
  if (command[0] == '\0') {
    return;
  }

  if (strcmp(command, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(command, "status") == 0) {
    lastDiagnosticsMs = 0;
    Serial.println("Status refresh requested.");
    return;
  }

  if (strcmp(command, "control") == 0) {
    Serial.print("Cooling enters at target delta: +");
    DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOnDeltaC);
    Serial.println(" C");
    Serial.print("Cooling enters fan-high at target delta: +");
    DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.highCoolingDeltaC);
    Serial.println(" C");
    Serial.print("Cooling leaves at target delta: ");
    DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOffDeltaC);
    Serial.println(" C");
    Serial.print("Fan-low PWM: ");
    Serial.print(runtimeControlConfig.fanLowPwmPercent);
    Serial.println("%");
    Serial.print("Fan-high PWM: ");
    Serial.print(runtimeControlConfig.fanHighPwmPercent);
    Serial.println("%");
    return;
  }

  if (strcmp(command, "faults") == 0) {
    printFaultPolicyDefaults();
    return;
  }

  if (strcmp(command, "network") == 0) {
    mqttTelemetry.printStatus(Serial);
    return;
  }

  if (strcmp(command, "ota status") == 0 || strcmp(command, "ota") == 0) {
    otaUploadServer.printStatus(Serial);
    return;
  }

  if (strcmp(command, "ota enable") == 0) {
    otaUploadServer.enable(millis(), Serial);
    return;
  }

  if (strcmp(command, "ota cancel") == 0) {
    otaUploadServer.cancel(Serial);
    return;
  }

  if (strcmp(command, "publish") == 0) {
    if (!lastFaultSnapshotValid) {
      Serial.println("Telemetry not ready yet. Wait for the next diagnostics cycle.");
      return;
      }

      const bool published = mqttTelemetry.publishTelemetry(millis(),
                                                            lastControlSnapshot,
                                                            runtimeControlConfig,
                                                            otaTelemetrySnapshot,
                                                            lastFaultSnapshot,
                                                            lastFaultPolicySnapshot,
                                                            remoteConfigStatus,
                                                            true);
      Serial.println(published ? "Telemetry published." :
                                "Telemetry not published. Check network status.");
    return;
  }

  if (strcmp(command, "default") == 0) {
    resetTargetTemperatureToDefault();
    Serial.print("Target temperature reset to default ");
    DisplayFormat::printTemperatureC(Serial, requestedTargetTemperatureC);
    Serial.println(" C.");
    return;
  }

  if (strncmp(command, "target ", 7) == 0) {
    char* endPtr = nullptr;
    const float parsedTargetTemperatureC = strtof(command + 7, &endPtr);
    if (*endPtr != '\0' || !setTargetTemperature(parsedTargetTemperatureC)) {
      resetTargetTemperatureToDefault();
      Serial.print("Invalid target temperature. Falling back to default ");
      DisplayFormat::printTemperatureC(Serial, requestedTargetTemperatureC);
      Serial.println(" C.");
      return;
    }

    Serial.print("Target temperature set to ");
    DisplayFormat::printTemperatureC(Serial, requestedTargetTemperatureC);
    Serial.println(" C and persisted.");
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
  printHelp();
}

void processSerialInput() {
  while (Serial.available() > 0) {
    const char incoming = (char)Serial.read();

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      serialCommandBuffer[serialCommandLength] = '\0';
      handleSerialCommand(serialCommandBuffer);
      serialCommandLength = 0;
      continue;
    }

    if (serialCommandLength + 1 >= kSerialCommandBufferSize) {
      serialCommandLength = 0;
      Serial.println("Command too long. Try 'status' or 'target 23.0'.");
      continue;
    }

    serialCommandBuffer[serialCommandLength++] = incoming;
  }
}

ControlInputs buildControlInputs(const SensorSnapshot& sensorSnapshot) {
  const TrackedSensorSnapshot& waterSensor =
      sensorSnapshot.trackedSensors[kWaterSensorIndex];

  return {
      hasConfiguredTargetTemperature,
      requestedTargetTemperatureC,
      waterSensor.sampleValid,
      waterSensor.temperatureC,
      lastControlSnapshot.mode,
  };
}

}  // namespace

/**
 * @brief Sets up hardware, persisted settings, diagnostics, and telemetry.
 *
 * Arduino calls this once after boot. The setup routine brings up serial output,
 * non-volatile preferences, fan PWM, RPM capture, the OneWire sensor bus, and
 * MQTT telemetry. It also prints the hardware configuration and measured fan
 * curve so an installed controller can be inspected from the serial monitor.
 */
void setup() {
  Serial.begin(115200);
  delay(1500);

  const bool preferencesOk = beginPreferences();
  initializeRuntimeControlConfig();
  loadPersistedTargetTemperature();
  const bool fanReady = fanDriver.begin();
  const bool rpmReady = rpmMonitor.begin();
  const bool sensorBusReady = sensorManager.begin(millis());
  const SensorSnapshot& sensorSnapshot = sensorManager.snapshot();
  const bool oledReady = oledStatusDisplay.begin(millis());
  oledStatusDisplay.update(millis(), sensorSnapshot, false, lastFaultPolicySnapshot);
  lastControlSnapshot =
      ControlEngine::compute(buildControlInputs(sensorSnapshot), runtimeControlConfig);
  confirmRunningOtaImageIfNeeded();

  Serial.println();
  Serial.println("Aquarium cooling controller");
  printCoreVersion();
  Serial.print("Fan driver init: ");
  Serial.println(fanReady ? "ok" : "failed");
  Serial.print("RPM monitor init: ");
  Serial.println(rpmReady ? "ok" : "failed");
  Serial.print("Sensor bus init: ");
  Serial.println(sensorBusReady ? "ok" : "failed");
  Serial.print("OLED display init: ");
  Serial.println(oledReady ? "ok" : "disabled");
  Serial.print("Preferences init: ");
  Serial.println(preferencesOk ? "ok" : "failed");
  Serial.print("1-Wire bus GPIO: ");
  Serial.println(kSensorManagerConfig.oneWirePin);
  printConfiguredRom("Configured water sensor ROM: ", kWaterSensorRomCode);
  printTemperatureLine("Default target temperature: ",
                       kDefaultControlConfig.defaultTargetTemperatureC);
  Serial.print("Loaded target source: ");
  Serial.println(hasConfiguredTargetTemperature ? "persisted/custom" : "default");
  printTemperatureLine("Fallback target temperature: ",
                       kDefaultControlConfig.defaultTargetTemperatureC);
  Serial.print("Water sensor fault fallback PWM: ");
  Serial.print(kDefaultControlConfig.fallbackPwmPercent);
  Serial.println("%");
  Serial.print("Cooling enters at target delta: +");
  DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOnDeltaC);
  Serial.println(" C");
  Serial.print("Cooling enters fan-high at target delta: +");
  DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.highCoolingDeltaC);
  Serial.println(" C");
  Serial.print("Cooling leaves at target delta: ");
  DisplayFormat::printTemperatureC(Serial, runtimeControlConfig.coolingOffDeltaC);
  Serial.println(" C");
  Serial.print("Fan-low PWM: ");
  Serial.print(runtimeControlConfig.fanLowPwmPercent);
  Serial.println("%");
  Serial.print("Fan-high PWM: ");
  Serial.print(runtimeControlConfig.fanHighPwmPercent);
  Serial.println("%");
  printTrackedSensorDetails(sensorSnapshot, millis());
  printDiscoveredBusSensors(sensorSnapshot);
  printCurveSummary();
  mqttTelemetry.setRemoteConfigCallback(handleRemoteConfigMessage, nullptr);
  mqttTelemetry.begin(millis());
  otaUploadServer.begin(
      kFirmwareName, kFirmwareVersion, kFirmwareIdentityTag, kFirmwareVersionTag);
  syncOtaTelemetrySnapshot();
  mqttTelemetry.printStatus(Serial);
  printHelp();
}

/**
 * @brief Runs one non-blocking controller iteration.
 *
 * Arduino calls this repeatedly. Each iteration services serial commands,
 * advances sensor sampling, recomputes the control snapshot, updates the fan
 * output and RPM monitor, and maintains MQTT connectivity. Diagnostics and
 * telemetry are emitted at kDiagnosticsIntervalMs so the controller loop remains
 * responsive between reporting cycles.
 */
void loop() {
  const uint32_t nowMs = millis();
  processSerialInput();

  sensorManager.update(nowMs);
  const SensorSnapshot& sensorSnapshot = sensorManager.snapshot();
  lastControlSnapshot =
      ControlEngine::compute(buildControlInputs(sensorSnapshot), runtimeControlConfig);

  fanDriver.setCommandedPwmPercent(lastControlSnapshot.finalPwmPercent, nowMs);
  fanDriver.update(nowMs);
  rpmMonitor.update(nowMs);
  mqttTelemetry.update(nowMs);
  otaUploadServer.update(nowMs);
  syncOtaTelemetrySnapshot();
  oledStatusDisplay.update(nowMs, sensorSnapshot, lastFaultSnapshotValid, lastFaultPolicySnapshot);

  if (telemetryPublishRequested && lastFaultSnapshotValid) {
    const bool published = mqttTelemetry.publishTelemetry(nowMs,
                                                          lastControlSnapshot,
                                                          runtimeControlConfig,
                                                          otaTelemetrySnapshot,
                                                          lastFaultSnapshot,
                                                          lastFaultPolicySnapshot,
                                                          remoteConfigStatus,
                                                          true);
    if (published) {
      markTelemetryPublishSuccessful(lastControlSnapshot,
                                     lastFaultSnapshot,
                                     lastFaultPolicySnapshot);
    }
  }

  if (nowMs - lastDiagnosticsMs < kDiagnosticsIntervalMs) {
    return;
  }

  lastDiagnosticsMs = nowMs;

  lastFaultSnapshot =
      faultMonitor.evaluate(fanDriver.appliedPwmPercent(), rpmMonitor.rpm(), nowMs);
  lastFaultPolicySnapshot = FaultPolicy::evaluate(lastControlSnapshot, lastFaultSnapshot);
  lastFaultSnapshotValid = true;

  const TelemetryEventState currentTelemetryEventState =
      buildTelemetryEventState(lastControlSnapshot,
                               lastFaultSnapshot,
                               lastFaultPolicySnapshot);
  if (lastPublishedTelemetryEventStateValid &&
      !telemetryEventStateEquals(currentTelemetryEventState,
                                 lastPublishedTelemetryEventState)) {
    requestTelemetryPublish();
  }

  printDiagnostics(lastFaultSnapshot,
                   lastFaultPolicySnapshot,
                   rpmMonitor.sampleAgeMs(nowMs),
                   nowMs);
  const bool published = mqttTelemetry.publishTelemetry(nowMs,
                                                        lastControlSnapshot,
                                                        runtimeControlConfig,
                                                        otaTelemetrySnapshot,
                                                        lastFaultSnapshot,
                                                        lastFaultPolicySnapshot,
                                                        remoteConfigStatus,
                                                        false);
  if (published) {
    markTelemetryPublishSuccessful(lastControlSnapshot,
                                   lastFaultSnapshot,
                                   lastFaultPolicySnapshot);
  }
}
