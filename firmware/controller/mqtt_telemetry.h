#pragma once

/**
 * @file mqtt_telemetry.h
 * @brief Wi-Fi and MQTT telemetry publisher for controller state.
 *
 * The telemetry layer is optional at runtime. Local cooling continues even when
 * Wi-Fi credentials are missing, the broker is unavailable, or MQTT publishing
 * fails.
 */

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "control_engine.h"
#include "fault_monitor.h"
#include "fault_policy.h"

/**
 * @brief Snapshot of operator-visible runtime settings that should be published.
 */
struct SettingsTelemetrySnapshot {
  bool airAssistEnabled;               ///< Active air-assist enable flag.
  uint8_t airAssistMinimumPwmPercent;  ///< Active minimum PWM for air assist.
};

/**
 * @brief Last-result summary for MQTT remote configuration commands.
 */
struct RemoteConfigStatus {
  bool lastCommandSeen;         ///< True once at least one remote command was processed.
  bool lastCommandAccepted;     ///< True when the last command was accepted.
  uint32_t acceptedCount;       ///< Total accepted remote commands since boot.
  uint32_t rejectedCount;       ///< Total rejected remote commands since boot.
  char lastKey[32];             ///< Key name of the last processed command.
  char lastDetail[96];          ///< Short apply/reject detail for the last command.
};

/**
 * @brief Maintains Wi-Fi/MQTT connections and publishes controller telemetry.
 *
 * Connection management is non-blocking from the caller's point of view: update()
 * performs periodic reconnect attempts while publishTelemetry() emits the latest
 * controller state when a broker connection is available. Topic names and
 * credentials come from the network configuration macros.
 */
class MqttTelemetry {
  public:
  using RemoteConfigCallback = void (*)(const char* suffix,
                                        const uint8_t* payload,
                                        size_t length,
                                        void* context);

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
   * The payload is split across stable topic suffixes so FHEM or other home
   * automation integrations can subscribe to individual state values. When
   * force is false, the method respects the configured publish interval.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @param controlSnapshot Latest control-engine state.
   * @param settingsSnapshot Latest configurable controller settings.
   * @param faultSnapshot Latest fan fault-monitor state.
   * @param policySnapshot Latest fault-policy state.
   * @param remoteConfigStatus Latest remote-configuration apply status.
   * @param force True to publish even before the normal interval elapses.
   * @return True when the publish attempt completed successfully.
   */
  bool publishTelemetry(uint32_t nowMs,
                        const ControlSnapshot& controlSnapshot,
                        const SettingsTelemetrySnapshot& settingsSnapshot,
                        const FaultMonitorSnapshot& faultSnapshot,
                        const FaultPolicySnapshot& policySnapshot,
                        const RemoteConfigStatus& remoteConfigStatus,
                        bool force);

  /**
   * @brief Registers a callback for validated MQTT /set topic dispatch.
   *
   * The telemetry layer only routes the subscribed topic suffix and raw payload
   * bytes. Validation and persistence remain in the sketch so local control
   * policy is owned by the main firmware orchestration.
   *
   * @param callback Callback invoked for supported remote configuration topics.
   * @param context Opaque caller-owned pointer forwarded to the callback.
   */
  void setRemoteConfigCallback(RemoteConfigCallback callback, void* context);

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
  bool subscribeRemoteConfigTopics();
  bool publishText(const char* suffix, const char* payload, bool retained);
  bool publishBool(const char* suffix, bool value, bool retained = false);
  bool publishUInt(const char* suffix, uint32_t value, bool retained = false);
  bool publishInt(const char* suffix, int32_t value, bool retained = false);
  bool publishFloatOrUnavailable(const char* suffix,
                                 bool valid,
                                 float value,
                                 bool retained = false);
  bool buildTopic(const char* suffix, char* topic, size_t topicSize) const;
  void handleMqttMessage(char* topic, const uint8_t* payload, unsigned int length);
  static void mqttMessageThunk(char* topic, uint8_t* payload, unsigned int length);

  WiFiClient wifiClient_;
  PubSubClient mqttClient_;
  RemoteConfigCallback remoteConfigCallback_;
  void* remoteConfigContext_;
  bool initialized_;
  bool wifiBeginIssued_;
  uint32_t lastWifiAttemptMs_;
  uint32_t lastMqttAttemptMs_;
  uint32_t lastPublishMs_;
};
