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

The verified local bench setup used an override root topic of
`aquarium_cooling` from the ignored `network_config.local.h`. Keep dashboards
and test commands aligned with the configured root.

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

## Windows MQTT Helper

The repository includes `tools/mqtt-client.ps1` for broker-side verification
from the development machine. It wraps `mosquitto_sub` and `mosquitto_pub` and
keeps optional credentials in a Windows DPAPI-protected file below
`%LOCALAPPDATA%`, outside the synced repository.

Example subscribe command:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\mqtt-client.ps1 -Mode sub -BrokerHost <broker-host> -RootTopic aquarium_cooling -Count 45
```

Local capture logs from the verification run were written below `build/`.
Those files are intentionally ignored and are not part of the committed
documentation.

## FHEM MQTT2 Integration

The repository includes a FHEM monitoring integration in:

```text
integrations/fhem/
```

The main file is:

```text
integrations/fhem/aquarium-cooling-mqtt2-device.cfg
```

It defines an `MQTT2_DEVICE` with readings for all currently published state,
diagnostic, status, and availability topics. The definition uses the verified
bench root topic `aquarium_cooling`. If the firmware uses the committed default
root topic `aquarium/cooling`, replace the root topic in the FHEM file before
importing it.

The FHEM integration is intentionally monitoring-only for this firmware
revision. A future `setList` for `target_temp_c`, `air_assist_enable`, and
`air_min_pwm_percent` is documented in the file but remains commented until the
firmware subscribes to and validates remote set topics.

## Verification Plan

1. Compile with no local network config. Done.
2. Flash and confirm local controller still starts with telemetry disabled. Done.
3. Add `network_config.local.h`, compile, and flash. Done.
4. Use `network` to verify Wi-Fi and MQTT state. Done.
5. Subscribe to the configured root topic on the broker. Done.
6. Use `publish` and verify all expected topics appear. Done.
7. Induce sensor and fan faults and confirm status topics follow the local
   fault policy. Done.
8. Confirm recovery returns status topics to `none` / normal operation. Done.

## Verified So Far

- Firmware compiles for `esp32:esp32:esp32`.
- `PubSubClient` 2.8, ESP32 `WiFi`, `OneWire`, and `DallasTemperature` are resolved by Arduino CLI.
- The flashed controller reports Wi-Fi connected, an assigned IP address,
  MQTT connected, root topic `aquarium_cooling`, and successful telemetry
  publishing through the `network` command.
- Normal broker capture received all 20 expected topics:
  `state/water_temp_c`, `state/air_temp_c`, `state/target_temp_c`,
  `state/fan_pwm_percent`, `state/fan_rpm`, `state/controller_mode`,
  `diagnostic/expected_rpm`, `diagnostic/rpm_tolerance`,
  `diagnostic/rpm_error`, `diagnostic/plausibility_active`,
  `status/fan_plausible`, `status/fan_fault`,
  `status/water_sensor_ok`, `status/air_sensor_ok`,
  `status/alarm_code`, `status/fault_severity`,
  `status/fault_response`, `status/cooling_degraded`,
  `status/service_required`, and `status/availability`.
- Normal payloads were plausible on the bench: availability `online`,
  `controller_mode=water-control`, `target_temp_c=23.00`, healthy water and
  air sensors, a measured fan RPM, expected RPM, RPM tolerance, RPM error, and
  `fan_plausible=true`.
- Air-sensor fault was observed on MQTT as `air-sensor-fault`,
  `fault_severity=warning`, `fault_response=disable-air-assist`,
  `air_sensor_ok=false`, and `service_required=true`.
- Water-sensor fault was observed on MQTT as `water-sensor-fault`,
  `fault_severity=critical`, `fault_response=water-fallback`,
  `water_sensor_ok=false`, `cooling_degraded=true`,
  `service_required=true`, and fallback fan PWM.
- Fan RPM deviation was observed on MQTT as `fan-fault`,
  `fault_severity=critical`, `fault_response=report-fan-fault`,
  `fan_fault=true`, `cooling_degraded=true`, and
  `service_required=true`.
- Fault recovery was observed on MQTT: `alarm_code=none`,
  `fault_severity=none`, `fault_response=normal-control`,
  `cooling_degraded=false`, and `service_required=false`.
- A FHEM `MQTT2_DEVICE` definition is available for the verified topic set,
  with the future remote-control `setList` kept commented until firmware
  support exists.
