/**
 * @file controller.ino
 * @brief Arduino sketch entry point for the aquarium cooling controller.
 *
 * The sketch wires the firmware modules together and owns the runtime loop. It
 * initializes persistent target-temperature storage, temperature sensing, fan
 * PWM output, tachometer RPM monitoring, fault evaluation, serial diagnostics,
 * and optional MQTT telemetry.
 *
 * Runtime behavior is intentionally local-first: cooling control, fan safety
 * monitoring, and diagnostics continue on the ESP32 even when Wi-Fi, MQTT, or
 * external integrations are unavailable.
 *
 * @section controller_circuit Circuit
 *
 * - DS18B20 water probe on the shared OneWire bus.
 * - DS18B20 air probe on the same OneWire bus for warm-air assist.
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
 * The serial monitor exposes bench and service commands for status output,
 * target-temperature changes, default reset, air-assist settings, fault-policy
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
#include <Preferences.h>

#include "control_engine.h"
#include "fan_driver.h"
#include "fan_curve.h"
#include "fault_monitor.h"
#include "fault_policy.h"
#include "mqtt_telemetry.h"
#include "rpm_monitor.h"
#include "sensor_manager.h"

namespace {

constexpr char kFirmwareName[] = "aq-cooling-controller";
constexpr char kFirmwareVersion[] = "0.1.0";
constexpr uint32_t kDiagnosticsIntervalMs = 2000;
constexpr size_t kSerialCommandBufferSize = 32;
constexpr size_t kSensorAddressBufferSize = 17;
constexpr size_t kWaterSensorIndex = 0;
constexpr size_t kAirSensorIndex = 1;
constexpr char kPreferencesNamespace[] = "controller";
constexpr char kKeyHasCustomTarget[] = "target_set";
constexpr char kKeyTargetTemperature[] = "target_c";

constexpr uint8_t kWaterSensorRomCode[8] = {
    0x28, 0x33, 0x38, 0x44, 0x05, 0x00, 0x00, 0xCB,
};

constexpr uint8_t kAirSensorRomCode[8] = {
    0x28, 0x24, 0x46, 0x44, 0x05, 0x00, 0x00, 0xDA,
};

constexpr SensorManagerConfig kSensorManagerConfig = {
    33,
    2000,
    12,
    2,
    {
        {
            true,
            {0x28, 0x33, 0x38, 0x44, 0x05, 0x00, 0x00, 0xCB},
            "Water sensor",
        },
        {
            true,
            {0x28, 0x24, 0x46, 0x44, 0x05, 0x00, 0x00, 0xDA},
            "Air sensor",
        },
    },
};

FanDriver fanDriver;
RpmMonitor rpmMonitor;
FaultMonitor faultMonitor;
SensorManager sensorManager(kSensorManagerConfig);
MqttTelemetry mqttTelemetry;
Preferences preferences;
ControlSnapshot lastControlSnapshot = {};
FaultMonitorSnapshot lastFaultSnapshot = {};
FaultPolicySnapshot lastFaultPolicySnapshot = {};
uint32_t lastDiagnosticsMs = 0;
float requestedTargetTemperatureC = kDefaultControlConfig.defaultTargetTemperatureC;
bool hasConfiguredTargetTemperature = false;
bool preferencesReady = false;
bool lastFaultSnapshotValid = false;
char serialCommandBuffer[kSerialCommandBufferSize] = {};
size_t serialCommandLength = 0;

void printHelp() {
  Serial.println("Serial commands:");
  Serial.println("  status        -> print diagnostics immediately");
  Serial.println("  target <c>    -> set target water temperature in C");
  Serial.println("  default       -> reset target temperature to default 23.0 C");
  Serial.println("  airassist     -> print current air-assist defaults");
  Serial.println("  faults        -> print current fault-policy defaults");
  Serial.println("  network       -> print Wi-Fi/MQTT telemetry status");
  Serial.println("  publish       -> publish telemetry immediately when MQTT is connected");
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

  Serial.print("Initial plausibility tolerance: +/-");
  Serial.print(FanCurve::kPlausibilityTolerancePercent);
  Serial.println("%");
}

bool beginPreferences() {
  preferencesReady = preferences.begin(kPreferencesNamespace, false);
  return preferencesReady;
}

void clearPersistedTargetTemperature() {
  if (!preferencesReady) {
    return;
  }

  preferences.remove(kKeyHasCustomTarget);
  preferences.remove(kKeyTargetTemperature);
}

void persistTargetTemperature(float targetTemperatureC) {
  if (!preferencesReady) {
    return;
  }

  preferences.putBool(kKeyHasCustomTarget, true);
  preferences.putFloat(kKeyTargetTemperature, targetTemperatureC);
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
      Serial.print(tracked.temperatureC, 2);
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

  Serial.print("  Target temperature: ");
  Serial.print(lastControlSnapshot.targetTemperatureC, 2);
  Serial.println(" C");

  Serial.print("  Target source: ");
  Serial.println(hasConfiguredTargetTemperature ? "persisted/custom" : "default");

  Serial.print("  Target defaulted: ");
  Serial.println(lastControlSnapshot.targetDefaulted ? "yes" : "no");

  Serial.print("  Water delta: ");
  if (lastControlSnapshot.waterSensorValid && isfinite(lastControlSnapshot.waterDeltaC)) {
    Serial.print(lastControlSnapshot.waterDeltaC, 2);
    Serial.println(" C");
  } else {
    Serial.println("n/a");
  }

  Serial.print("  Water-based PWM: ");
  Serial.print(lastControlSnapshot.waterBasedPwmPercent);
  Serial.println("%");

  Serial.print("  Air-based PWM: ");
  Serial.print(lastControlSnapshot.airBasedPwmPercent);
  Serial.println("%");

  Serial.print("  Air assist enabled: ");
  Serial.println(kDefaultControlConfig.airAssistEnabled ? "yes" : "no");

  Serial.print("  Air assist threshold: ");
  Serial.print(kDefaultControlConfig.airAssistStartTemperatureC, 2);
  Serial.println(" C");

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

  Serial.print("  Air sensor fault response: ");
  Serial.println(FaultPolicy::responseLabel(FaultResponse::kDisableAirAssist));

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

  Serial.print("  Air sensor ok: ");
  Serial.println(policySnapshot.airSensorOk ? "yes" : "no");

  Serial.print("  Fan ok: ");
  Serial.println(policySnapshot.fanOk ? "yes" : "no");

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

  if (strcmp(command, "airassist") == 0) {
    Serial.print("Air assist enabled: ");
    Serial.println(kDefaultControlConfig.airAssistEnabled ? "yes" : "no");
    Serial.print("Air assist start temperature: ");
    Serial.print(kDefaultControlConfig.airAssistStartTemperatureC, 2);
    Serial.println(" C");
    Serial.print("Air assist full temperature: ");
    Serial.print(kDefaultControlConfig.airAssistFullTemperatureC, 2);
    Serial.println(" C");
    Serial.print("Air assist PWM range: ");
    Serial.print(kDefaultControlConfig.airAssistMinimumPwmPercent);
    Serial.print("% .. ");
    Serial.print(kDefaultControlConfig.airAssistMaximumPwmPercent);
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

  if (strcmp(command, "publish") == 0) {
    if (!lastFaultSnapshotValid) {
      Serial.println("Telemetry not ready yet. Wait for the first diagnostics cycle.");
      return;
    }

    const bool published = mqttTelemetry.publishTelemetry(millis(),
                                                          lastControlSnapshot,
                                                          lastFaultSnapshot,
                                                          lastFaultPolicySnapshot,
                                                          true);
    Serial.println(published ? "Telemetry published." :
                              "Telemetry not published. Check network status.");
    return;
  }

  if (strcmp(command, "default") == 0) {
    requestedTargetTemperatureC = kDefaultControlConfig.defaultTargetTemperatureC;
    hasConfiguredTargetTemperature = false;
    clearPersistedTargetTemperature();
    Serial.print("Target temperature reset to default ");
    Serial.print(requestedTargetTemperatureC, 2);
    Serial.println(" C.");
    return;
  }

  if (strncmp(command, "target ", 7) == 0) {
    char* endPtr = nullptr;
    const float parsedTargetTemperatureC = strtof(command + 7, &endPtr);
    if (*endPtr != '\0' ||
        !ControlEngine::isTargetTemperatureValid(parsedTargetTemperatureC)) {
      requestedTargetTemperatureC = kDefaultControlConfig.defaultTargetTemperatureC;
      hasConfiguredTargetTemperature = false;
      clearPersistedTargetTemperature();
      Serial.print("Invalid target temperature. Falling back to default ");
      Serial.print(requestedTargetTemperatureC, 2);
      Serial.println(" C.");
      return;
    }

    requestedTargetTemperatureC = parsedTargetTemperatureC;
    hasConfiguredTargetTemperature = true;
    persistTargetTemperature(requestedTargetTemperatureC);
    Serial.print("Target temperature set to ");
    Serial.print(requestedTargetTemperatureC, 2);
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
  const TrackedSensorSnapshot& airSensor =
      sensorSnapshot.trackedSensors[kAirSensorIndex];

  return {
      hasConfiguredTargetTemperature,
      requestedTargetTemperatureC,
      waterSensor.sampleValid,
      waterSensor.temperatureC,
      airSensor.sampleValid,
      airSensor.temperatureC,
  };
}

}  // namespace

/**
 * @brief Initializes hardware, persisted settings, diagnostics, and telemetry.
 *
 * Arduino calls this once after boot. The setup routine brings up serial output,
 * non-volatile preferences, fan PWM, RPM capture, the OneWire sensor bus, and
 * MQTT telemetry. It also prints the initial hardware configuration and the
 * measured fan curve so bench bring-up can be verified from the serial monitor.
 */
void setup() {
  Serial.begin(115200);
  delay(1500);

  const bool preferencesOk = beginPreferences();
  loadPersistedTargetTemperature();
  const bool fanReady = fanDriver.begin();
  const bool rpmReady = rpmMonitor.begin();
  const bool sensorBusReady = sensorManager.begin(millis());
  const SensorSnapshot& sensorSnapshot = sensorManager.snapshot();
  lastControlSnapshot = ControlEngine::compute(buildControlInputs(sensorSnapshot));

  Serial.println();
  Serial.println("Aquarium cooling controller");
  printCoreVersion();
  Serial.print("Fan driver init: ");
  Serial.println(fanReady ? "ok" : "failed");
  Serial.print("RPM monitor init: ");
  Serial.println(rpmReady ? "ok" : "failed");
  Serial.print("Sensor bus init: ");
  Serial.println(sensorBusReady ? "ok" : "failed");
  Serial.print("Preferences init: ");
  Serial.println(preferencesOk ? "ok" : "failed");
  Serial.print("1-Wire bus GPIO: ");
  Serial.println(kSensorManagerConfig.oneWirePin);
  printConfiguredRom("Configured water sensor ROM: ", kWaterSensorRomCode);
  printConfiguredRom("Configured air sensor ROM: ", kAirSensorRomCode);
  Serial.print("Default target temperature: ");
  Serial.print(kDefaultControlConfig.defaultTargetTemperatureC, 2);
  Serial.println(" C");
  Serial.print("Loaded target source: ");
  Serial.println(hasConfiguredTargetTemperature ? "persisted/custom" : "default");
  Serial.print("Fallback target temperature: ");
  Serial.print(kDefaultControlConfig.defaultTargetTemperatureC, 2);
  Serial.println(" C");
  Serial.print("Water sensor fault fallback PWM: ");
  Serial.print(kDefaultControlConfig.fallbackPwmPercent);
  Serial.println("%");
  Serial.print("Air assist start temperature: ");
  Serial.print(kDefaultControlConfig.airAssistStartTemperatureC, 2);
  Serial.println(" C");
  Serial.print("Air assist PWM range: ");
  Serial.print(kDefaultControlConfig.airAssistMinimumPwmPercent);
  Serial.print("% .. ");
  Serial.print(kDefaultControlConfig.airAssistMaximumPwmPercent);
  Serial.println("%");
  printTrackedSensorDetails(sensorSnapshot, millis());
  printDiscoveredBusSensors(sensorSnapshot);
  printCurveSummary();
  mqttTelemetry.begin(millis());
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
  lastControlSnapshot = ControlEngine::compute(buildControlInputs(sensorSnapshot));

  fanDriver.setCommandedPwmPercent(lastControlSnapshot.finalPwmPercent, nowMs);
  fanDriver.update(nowMs);
  rpmMonitor.update(nowMs);
  mqttTelemetry.update(nowMs);

  if (nowMs - lastDiagnosticsMs < kDiagnosticsIntervalMs) {
    return;
  }

  lastDiagnosticsMs = nowMs;

  lastFaultSnapshot =
      faultMonitor.evaluate(fanDriver.appliedPwmPercent(), rpmMonitor.rpm(), nowMs);
  lastFaultPolicySnapshot = FaultPolicy::evaluate(lastControlSnapshot, lastFaultSnapshot);
  lastFaultSnapshotValid = true;

  printDiagnostics(lastFaultSnapshot,
                   lastFaultPolicySnapshot,
                   rpmMonitor.sampleAgeMs(nowMs),
                   nowMs);
  mqttTelemetry.publishTelemetry(nowMs,
                                 lastControlSnapshot,
                                 lastFaultSnapshot,
                                 lastFaultPolicySnapshot,
                                 false);
}
