#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "control_engine.h"
#include "fault_monitor.h"
#include "fault_policy.h"

class MqttTelemetry {
 public:
  MqttTelemetry();

  void begin(uint32_t nowMs);
  void update(uint32_t nowMs);
  bool publishTelemetry(uint32_t nowMs,
                        const ControlSnapshot& controlSnapshot,
                        const FaultMonitorSnapshot& faultSnapshot,
                        const FaultPolicySnapshot& policySnapshot,
                        bool force);
  void printStatus(Stream& out);

  bool enabled() const;
  bool wifiConnected() const;
  bool mqttConnected();

 private:
  bool configComplete() const;
  void maintainWifi(uint32_t nowMs);
  void maintainMqtt(uint32_t nowMs);
  bool connectMqtt();
  bool publishText(const char* suffix, const char* payload, bool retained);
  bool publishBool(const char* suffix, bool value, bool retained = false);
  bool publishUInt(const char* suffix, uint32_t value, bool retained = false);
  bool publishInt(const char* suffix, int32_t value, bool retained = false);
  bool publishFloatOrUnavailable(const char* suffix,
                                 bool valid,
                                 float value,
                                 bool retained = false);
  bool buildTopic(const char* suffix, char* topic, size_t topicSize) const;

  WiFiClient wifiClient_;
  PubSubClient mqttClient_;
  bool initialized_;
  bool wifiBeginIssued_;
  uint32_t lastWifiAttemptMs_;
  uint32_t lastMqttAttemptMs_;
  uint32_t lastPublishMs_;
};
