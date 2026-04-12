#include <Arduino.h>

#include <esp_arduino_version.h>

#include "fan_driver.h"
#include "fan_curve.h"
#include "fault_monitor.h"
#include "rpm_monitor.h"

namespace {

constexpr uint32_t kDiagnosticsIntervalMs = 2000;
constexpr size_t kSerialCommandBufferSize = 16;

FanDriver fanDriver;
RpmMonitor rpmMonitor;
FaultMonitor faultMonitor;
uint32_t lastDiagnosticsMs = 0;
uint8_t targetPwmPercent = 0;
char serialCommandBuffer[kSerialCommandBufferSize] = {};
size_t serialCommandLength = 0;

void printHelp() {
  Serial.println("Serial commands:");
  Serial.println("  0..100  -> set target PWM percent");
  Serial.println("  stop    -> set PWM to 0%");
  Serial.println("  status  -> print current diagnostics immediately");
  Serial.println("  help    -> show this help");
  Serial.println();
}

void printCoreVersion() {
  Serial.print("ESP32 Arduino core: ");
  Serial.print(ESP_ARDUINO_VERSION_MAJOR);
  Serial.print(".");
  Serial.print(ESP_ARDUINO_VERSION_MINOR);
  Serial.print(".");
  Serial.println(ESP_ARDUINO_VERSION_PATCH);
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

void printDiagnostics(const FaultMonitorSnapshot& snapshot, uint32_t sampleAgeMs) {
  Serial.println("Controller diagnostics:");

  Serial.print("  Target PWM: ");
  Serial.print(targetPwmPercent);
  Serial.println("%");

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

  Serial.print("  Start boost active: ");
  Serial.println(fanDriver.isStartBoostActive() ? "yes" : "no");

  Serial.print("  RPM sample age: ");
  Serial.print(sampleAgeMs);
  Serial.println(" ms");

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

  if (strcmp(command, "stop") == 0) {
    targetPwmPercent = 0;
    Serial.println("Target PWM set to 0%.");
    return;
  }

  if (strcmp(command, "status") == 0) {
    lastDiagnosticsMs = 0;
    Serial.println("Status refresh requested.");
    return;
  }

  char* endPtr = nullptr;
  const long pwmPercent = strtol(command, &endPtr, 10);
  if (*endPtr != '\0' || pwmPercent < 0 || pwmPercent > 100) {
    Serial.print("Unknown command: ");
    Serial.println(command);
    printHelp();
    return;
  }

  targetPwmPercent = (uint8_t)pwmPercent;
  Serial.print("Target PWM set to ");
  Serial.print(targetPwmPercent);
  Serial.println("%.");
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
      Serial.println("Command too long. Try a short command like 35 or stop.");
      continue;
    }

    serialCommandBuffer[serialCommandLength++] = incoming;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);

  const bool fanReady = fanDriver.begin();
  const bool rpmReady = rpmMonitor.begin();

  Serial.println();
  Serial.println("Aquarium cooling controller");
  printCoreVersion();
  Serial.print("Fan driver init: ");
  Serial.println(fanReady ? "ok" : "failed");
  Serial.print("RPM monitor init: ");
  Serial.println(rpmReady ? "ok" : "failed");
  printCurveSummary();
  printHelp();
}

void loop() {
  const uint32_t nowMs = millis();
  processSerialInput();

  fanDriver.setCommandedPwmPercent(targetPwmPercent, nowMs);
  fanDriver.update(nowMs);
  rpmMonitor.update(nowMs);

  if (nowMs - lastDiagnosticsMs < kDiagnosticsIntervalMs) {
    return;
  }

  lastDiagnosticsMs = nowMs;

  const FaultMonitorSnapshot snapshot =
      faultMonitor.evaluate(fanDriver.appliedPwmPercent(), rpmMonitor.rpm(), nowMs);

  printDiagnostics(snapshot, rpmMonitor.sampleAgeMs(nowMs));
}
