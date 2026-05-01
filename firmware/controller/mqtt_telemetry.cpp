/**
 * @file mqtt_telemetry.cpp
 * @brief Implements Wi-Fi/MQTT connection maintenance and telemetry publishing.
 */

#include "mqtt_telemetry.h"

#include <string.h>

#include "display_format.h"
#include "network_config.h"

namespace {

constexpr size_t kTopicBufferSize = 96;
constexpr size_t kPayloadBufferSize = 32;
constexpr char kSetTargetTemperatureSuffix[] = "/set/target_temp_c";
constexpr char kSetOtaEnableSuffix[] = "/set/ota_enable";

MqttTelemetry* gActiveMqttTelemetry = nullptr;

const char* wifiStatusLabel(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "idle";
    case WL_NO_SSID_AVAIL:
      return "ssid-unavailable";
    case WL_SCAN_COMPLETED:
      return "scan-completed";
    case WL_CONNECTED:
      return "connected";
    case WL_CONNECT_FAILED:
      return "connect-failed";
    case WL_CONNECTION_LOST:
      return "connection-lost";
    case WL_DISCONNECTED:
      return "disconnected";
    default:
      return "unknown";
  }
}

}  // namespace

MqttTelemetry::MqttTelemetry()
    : wifiClient_(),
      mqttClient_(wifiClient_),
      remoteConfigCallback_(nullptr),
      remoteConfigContext_(nullptr),
      initialized_(false),
      wifiBeginIssued_(false),
      lastWifiAttemptMs_(0),
      lastMqttAttemptMs_(0),
      lastPublishMs_(0) {}

void MqttTelemetry::begin(uint32_t nowMs) {
  initialized_ = true;
  lastWifiAttemptMs_ = nowMs - AQ_WIFI_RECONNECT_INTERVAL_MS;
  lastMqttAttemptMs_ = nowMs - AQ_MQTT_RECONNECT_INTERVAL_MS;
  lastPublishMs_ = nowMs - AQ_MQTT_PUBLISH_INTERVAL_MS;

  if (!enabled()) {
    return;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(AQ_DEVICE_HOSTNAME);

  mqttClient_.setServer(AQ_MQTT_HOST, AQ_MQTT_PORT);
  mqttClient_.setKeepAlive(15);
  mqttClient_.setSocketTimeout(2);
  mqttClient_.setCallback(MqttTelemetry::mqttMessageThunk);
  gActiveMqttTelemetry = this;
}

void MqttTelemetry::update(uint32_t nowMs) {
  if (!enabled()) {
    return;
  }

  maintainWifi(nowMs);

  if (!wifiConnected()) {
    return;
  }

  maintainMqtt(nowMs);

  if (mqttConnected()) {
    mqttClient_.loop();
  }
}

bool MqttTelemetry::publishTelemetry(uint32_t nowMs,
                                     const ControlSnapshot& controlSnapshot,
                                     const OtaTelemetrySnapshot& otaSnapshot,
                                     const FaultMonitorSnapshot& faultSnapshot,
                                     const FaultPolicySnapshot& policySnapshot,
                                     const RemoteConfigStatus& remoteConfigStatus,
                                     bool force) {
  if (!enabled() || !mqttConnected()) {
    return false;
  }

  if (!force && nowMs - lastPublishMs_ < AQ_MQTT_PUBLISH_INTERVAL_MS) {
    return false;
  }

  bool ok = true;
  ok &= publishFloatOrUnavailable("/state/water_temp_c",
                                  controlSnapshot.waterSensorValid,
                                  controlSnapshot.waterTemperatureC);
  ok &= publishFloatOrUnavailable("/state/air_temp_c",
                                  controlSnapshot.airSensorValid,
                                  controlSnapshot.airTemperatureC);
  ok &= publishFloatOrUnavailable("/state/target_temp_c",
                                  true,
                                  controlSnapshot.targetTemperatureC,
                                  true);
  ok &= publishUInt("/state/fan_pwm_percent", controlSnapshot.finalPwmPercent);
  ok &= publishUInt("/state/fan_rpm", faultSnapshot.measuredRpm);
  ok &= publishText("/state/controller_mode",
                    ControlEngine::modeLabel(controlSnapshot.mode),
                    true);

  ok &= publishUInt("/diagnostic/expected_rpm", faultSnapshot.expectedRpm);
  ok &= publishUInt("/diagnostic/rpm_tolerance", faultSnapshot.toleranceRpm);
  ok &= publishInt("/diagnostic/rpm_error", faultSnapshot.rpmError);
  ok &= publishBool("/diagnostic/plausibility_active",
                    faultSnapshot.plausibilityActive);
  ok &= publishBool("/status/fan_plausible", faultSnapshot.plausible);
  ok &= publishBool("/status/fan_fault", faultSnapshot.faultLatched, true);

  ok &= publishBool("/status/water_sensor_ok", policySnapshot.waterSensorOk, true);
  ok &= publishBool("/status/air_sensor_ok", policySnapshot.airSensorOk, true);
  ok &= publishBool("/status/cooling_degraded",
                    policySnapshot.coolingDegraded,
                    true);
  ok &= publishBool("/status/service_required",
                    policySnapshot.serviceRequired,
                    true);
  ok &= publishText("/status/alarm_code",
                    FaultPolicy::alarmCodeLabel(policySnapshot.alarmCode),
                    true);
  ok &= publishText("/status/fault_severity",
                    FaultPolicy::severityLabel(policySnapshot.severity),
                    true);
  ok &= publishText("/status/fault_response",
                    FaultPolicy::responseLabel(policySnapshot.response),
                    true);
  ok &= publishText("/status/firmware_version", otaSnapshot.firmwareVersion, true);
  ok &= publishText("/status/network_ip", otaSnapshot.networkIp, true);
  ok &= publishText("/status/ota_state", otaSnapshot.stateLabel, true);
  ok &= publishText("/status/ota_message", otaSnapshot.lastMessage, true);
  ok &= publishBool("/status/ota_window_active", otaSnapshot.active, true);
  ok &= publishText("/status/ota_upload_url", otaSnapshot.uploadUrl, true);
  ok &= publishText("/status/remote_config_last_result",
                    remoteConfigStatus.lastCommandSeen
                        ? (remoteConfigStatus.lastCommandAccepted ? "accepted" : "rejected")
                        : "none",
                    true);
  ok &= publishText("/status/remote_config_last_key",
                    remoteConfigStatus.lastCommandSeen ? remoteConfigStatus.lastKey : "none",
                    true);
  ok &= publishText(
      "/status/remote_config_last_detail",
      remoteConfigStatus.lastCommandSeen ? remoteConfigStatus.lastDetail : "none",
      true);
  ok &= publishText("/status/availability", "online", true);
  ok &= publishUInt("/diagnostic/remote_config_accept_count",
                    remoteConfigStatus.acceptedCount,
                    true);
  ok &= publishUInt("/diagnostic/remote_config_reject_count",
                    remoteConfigStatus.rejectedCount,
                    true);

  if (ok) {
    lastPublishMs_ = nowMs;
  }

  return ok;
}

void MqttTelemetry::setRemoteConfigCallback(RemoteConfigCallback callback,
                                            void* context) {
  remoteConfigCallback_ = callback;
  remoteConfigContext_ = context;
}

void MqttTelemetry::printStatus(Stream& out) {
  out.println("Network telemetry:");
  out.print("  Config complete: ");
  out.println(configComplete() ? "yes" : "no");
  out.print("  Enabled: ");
  out.println(enabled() ? "yes" : "no");
  out.print("  Wi-Fi SSID configured: ");
  out.println(strlen(AQ_WIFI_SSID) > 0 ? "yes" : "no");
  out.print("  Wi-Fi status: ");
  out.println(wifiStatusLabel(WiFi.status()));
  out.print("  Wi-Fi IP: ");
  if (wifiConnected()) {
    out.println(WiFi.localIP());
  } else {
    out.println("n/a");
  }
  out.print("  MQTT host configured: ");
  out.println(strlen(AQ_MQTT_HOST) > 0 ? "yes" : "no");
  out.print("  MQTT connected: ");
  out.println(mqttConnected() ? "yes" : "no");
  out.print("  MQTT root topic: ");
  out.println(AQ_MQTT_ROOT_TOPIC);
  out.print("  Publish interval: ");
  out.print(AQ_MQTT_PUBLISH_INTERVAL_MS);
  out.println(" ms");
  out.print("  Remote config subscribed: ");
  out.println(remoteConfigCallback_ != nullptr && mqttConnected() ? "yes" : "no");
}

bool MqttTelemetry::enabled() const {
  return initialized_ && AQ_NETWORK_ENABLED && configComplete();
}

bool MqttTelemetry::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool MqttTelemetry::mqttConnected() {
  return mqttClient_.connected();
}

bool MqttTelemetry::configComplete() const {
  return strlen(AQ_WIFI_SSID) > 0 && strlen(AQ_MQTT_HOST) > 0;
}

void MqttTelemetry::maintainWifi(uint32_t nowMs) {
  if (wifiConnected()) {
    return;
  }

  if (nowMs - lastWifiAttemptMs_ < AQ_WIFI_RECONNECT_INTERVAL_MS) {
    return;
  }

  lastWifiAttemptMs_ = nowMs;

  if (wifiBeginIssued_) {
    WiFi.disconnect(false);
  }

  WiFi.begin(AQ_WIFI_SSID, AQ_WIFI_PASSWORD);
  wifiBeginIssued_ = true;
}

void MqttTelemetry::maintainMqtt(uint32_t nowMs) {
  if (mqttConnected()) {
    return;
  }

  if (nowMs - lastMqttAttemptMs_ < AQ_MQTT_RECONNECT_INTERVAL_MS) {
    return;
  }

  lastMqttAttemptMs_ = nowMs;
  connectMqtt();
}

bool MqttTelemetry::connectMqtt() {
  char availabilityTopic[kTopicBufferSize] = {};
  if (!buildTopic("/status/availability",
                  availabilityTopic,
                  sizeof(availabilityTopic))) {
    return false;
  }

  bool connected = false;
  if (strlen(AQ_MQTT_USERNAME) > 0) {
    connected = mqttClient_.connect(AQ_MQTT_CLIENT_ID,
                                    AQ_MQTT_USERNAME,
                                    AQ_MQTT_PASSWORD,
                                    availabilityTopic,
                                    0,
                                    true,
                                    "offline");
  } else {
    connected = mqttClient_.connect(AQ_MQTT_CLIENT_ID,
                                    availabilityTopic,
                                    0,
                                    true,
                                    "offline");
  }

  if (connected) {
    if (!subscribeRemoteConfigTopics()) {
      mqttClient_.disconnect();
      return false;
    }

    publishText("/status/availability", "online", true);
  }

  return connected;
}

bool MqttTelemetry::subscribeRemoteConfigTopics() {
  if (remoteConfigCallback_ == nullptr) {
    return true;
  }

  char topic[kTopicBufferSize] = {};
  if (!buildTopic(kSetTargetTemperatureSuffix, topic, sizeof(topic)) ||
      !mqttClient_.subscribe(topic)) {
    return false;
  }

  if (!buildTopic(kSetOtaEnableSuffix, topic, sizeof(topic)) ||
      !mqttClient_.subscribe(topic)) {
    return false;
  }

  return true;
}

bool MqttTelemetry::publishText(const char* suffix,
                                const char* payload,
                                bool retained) {
  char topic[kTopicBufferSize] = {};
  if (!buildTopic(suffix, topic, sizeof(topic))) {
    return false;
  }

  return mqttClient_.publish(topic, payload, retained);
}

bool MqttTelemetry::publishBool(const char* suffix, bool value, bool retained) {
  return publishText(suffix, value ? "true" : "false", retained);
}

bool MqttTelemetry::publishUInt(const char* suffix,
                                uint32_t value,
                                bool retained) {
  char payload[kPayloadBufferSize] = {};
  snprintf(payload, sizeof(payload), "%lu", (unsigned long)value);
  return publishText(suffix, payload, retained);
}

bool MqttTelemetry::publishInt(const char* suffix, int32_t value, bool retained) {
  char payload[kPayloadBufferSize] = {};
  snprintf(payload, sizeof(payload), "%ld", (long)value);
  return publishText(suffix, payload, retained);
}

bool MqttTelemetry::publishFloatOrUnavailable(const char* suffix,
                                              bool valid,
                                              float value,
                                              bool retained) {
  if (!valid || !isfinite(value)) {
    return publishText(suffix, "unavailable", retained);
  }

  char payload[kPayloadBufferSize] = {};
  DisplayFormat::formatTemperatureC(value, payload, sizeof(payload));
  return publishText(suffix, payload, retained);
}

bool MqttTelemetry::buildTopic(const char* suffix,
                               char* topic,
                               size_t topicSize) const {
  const int written = snprintf(topic, topicSize, "%s%s", AQ_MQTT_ROOT_TOPIC, suffix);
  return written > 0 && (size_t)written < topicSize;
}

void MqttTelemetry::handleMqttMessage(char* topic,
                                      const uint8_t* payload,
                                      unsigned int length) {
  if (remoteConfigCallback_ == nullptr || topic == nullptr) {
    return;
  }

  char expectedTopic[kTopicBufferSize] = {};
  if (buildTopic(kSetTargetTemperatureSuffix, expectedTopic, sizeof(expectedTopic)) &&
      strcmp(topic, expectedTopic) == 0) {
    remoteConfigCallback_(kSetTargetTemperatureSuffix,
                          payload,
                          (size_t)length,
                          remoteConfigContext_);
    return;
  }

  if (buildTopic(kSetOtaEnableSuffix, expectedTopic, sizeof(expectedTopic)) &&
      strcmp(topic, expectedTopic) == 0) {
    remoteConfigCallback_(kSetOtaEnableSuffix,
                          payload,
                          (size_t)length,
                          remoteConfigContext_);
  }
}

void MqttTelemetry::mqttMessageThunk(char* topic,
                                     uint8_t* payload,
                                     unsigned int length) {
  if (gActiveMqttTelemetry == nullptr) {
    return;
  }

  gActiveMqttTelemetry->handleMqttMessage(topic, payload, length);
}
