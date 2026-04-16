# MQTT Telemetry - 2026-04-16

## Goal

Add Wi-Fi and MQTT observability without making local cooling dependent on
network services. The ESP32 must continue sensor sampling, PWM control, RPM
monitoring, and fault handling if Wi-Fi or the MQTT broker is unavailable.

## Implementation

The controller now contains a dedicated `mqtt_telemetry` module. It handles:

- non-blocking Wi-Fi reconnect attempts
- MQTT reconnect attempts with short socket timeout
- periodic telemetry publishing
- MQTT last-will availability state
- serial status output through the `network` command
- manual publish trigger through the `publish` command

The local control loop remains authoritative. MQTT is publish-only in this
step; remote set commands are deliberately left for a later branch.

## Configuration

Committed defaults live in `firmware/controller/network_config.h` and contain
no secrets.

For local testing, copy:

```text
firmware/controller/network_config.local.example.h
```

to:

```text
firmware/controller/network_config.local.h
```

Then fill in Wi-Fi and MQTT broker values. The local file is ignored by Git.

Required values:

- `AQ_WIFI_SSID`
- `AQ_WIFI_PASSWORD`
- `AQ_MQTT_HOST`

Optional values:

- `AQ_MQTT_PORT`
- `AQ_MQTT_USERNAME`
- `AQ_MQTT_PASSWORD`
- `AQ_MQTT_ROOT_TOPIC`

If SSID or broker host is empty, MQTT telemetry stays disabled and the
controller still compiles and runs locally.

## Published Topics

Default root topic: `aquarium/cooling`

| Topic suffix | Payload |
|---|---|
| `/state/water_temp_c` | water temperature or `unavailable` |
| `/state/air_temp_c` | air temperature or `unavailable` |
| `/state/target_temp_c` | active target temperature |
| `/state/fan_pwm_percent` | final commanded fan PWM |
| `/state/fan_rpm` | measured fan RPM |
| `/state/controller_mode` | local control mode |
| `/diagnostic/expected_rpm` | interpolated expected RPM |
| `/diagnostic/rpm_tolerance` | current RPM tolerance |
| `/diagnostic/rpm_error` | measured minus expected RPM |
| `/diagnostic/plausibility_active` | whether RPM plausibility is active |
| `/status/fan_plausible` | current plausibility result |
| `/status/fan_fault` | latched fan fault |
| `/status/water_sensor_ok` | water sensor health |
| `/status/air_sensor_ok` | air sensor health |
| `/status/cooling_degraded` | degraded cooling flag |
| `/status/service_required` | service required flag |
| `/status/alarm_code` | summarized fault code |
| `/status/fault_severity` | `none`, `warning`, or `critical` |
| `/status/fault_response` | local fault response |
| `/status/availability` | `online` or MQTT last-will `offline` |

## Serial Commands

| Command | Purpose |
|---|---|
| `network` | Print configuration completeness, Wi-Fi state, IP, MQTT state, root topic, and publish interval |
| `publish` | Publish telemetry immediately if MQTT is connected and a diagnostics snapshot exists |

## Verification Plan

1. Compile with no local network config.
2. Flash and confirm local controller still starts with telemetry disabled.
3. Add `network_config.local.h`, compile, and flash.
4. Use `network` to verify Wi-Fi and MQTT state.
5. Subscribe to `aquarium/cooling/#` on the broker.
6. Use `publish` and verify all expected topics appear.
7. Disconnect broker or Wi-Fi and confirm local cooling diagnostics continue.
8. Restore broker/Wi-Fi and confirm telemetry resumes.

## Verified So Far

- Firmware compiles for `esp32:esp32:esp32`.
- `PubSubClient` 2.8, ESP32 `WiFi`, `OneWire`, and `DallasTemperature` are resolved by Arduino CLI.
- No broker-side hardware test has been run yet in this branch.
