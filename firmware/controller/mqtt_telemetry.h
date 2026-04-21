#pragma once

/**
 * @file mqtt_telemetry.h
 * @brief Wi-Fi and MQTT telemetry publisher for controller state.
 */

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "control_engine.h"
#include "fault_monitor.h"
#include "fault_policy.h"

/**
 * @brief Maintains Wi-Fi/MQTT connections and publishes controller telemetry.
 */
class MqttTelemetry {
 public:
  /**
   * @brief Creates an uninitialized telemetry publisher.
   */
  MqttTelemetry();

  /**
   * @brief Starts telemetry connection management.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void begin(uint32_t nowMs);

  /**
   * @brief Maintains Wi-Fi and MQTT reconnect attempts.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   */
  void update(uint32_t nowMs);

  /**
   * @brief Publishes a complete telemetry snapshot when connected.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @param controlSnapshot Latest control-engine state.
   * @param faultSnapshot Latest fan fault-monitor state.
   * @param policySnapshot Latest fault-policy state.
   * @param force True to publish even before the normal interval elapses.
   * @return True when the publish attempt completed successfully.
   */
  bool publishTelemetry(uint32_t nowMs,
                        const ControlSnapshot& controlSnapshot,
                        const FaultMonitorSnapshot& faultSnapshot,
                        const FaultPolicySnapshot& policySnapshot,
                        bool force);

  /**
   * @brief Prints current Wi-Fi/MQTT status to a stream.
   *
   * @param out Destination stream.
   */
  void printStatus(Stream& out);

  /**
   * @brief Indicates whether network telemetry is configured and enabled.
   *
   * @return True when telemetry should attempt Wi-Fi/MQTT operation.
   */
  bool enabled() const;

  /**
   * @brief Indicates whether Wi-Fi is currently connected.
   *
   * @return True when Wi-Fi is connected.
   */
  bool wifiConnected() const;

  /**
   * @brief Indicates whether MQTT is currently connected.
   *
   * @return True when the MQTT client is connected.
   */
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
