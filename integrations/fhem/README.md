# FHEM MQTT2 Integration

This directory contains the FHEM definition for observing the aquarium cooling
controller and sending validated staged-control settings through MQTT.

## Current Scope

The target control surface publishes controller state, diagnostics,
availability, and fault-policy readings, and it subscribes to a validated
`/set/...` surface for:

- `target_temp_c`
- `cooling_on_delta_c`
- `cooling_off_delta_c`
- `high_cooling_delta_c`
- `fan_low_pwm_percent`
- `fan_high_pwm_percent`
- `ota_enable`

Local cooling on the ESP32 remains authoritative. FHEM is allowed to adjust
only these validated persisted settings.

## Files

| File | Purpose |
|---|---|
| `aquarium-cooling-mqtt2-device.cfg` | Pasteable FHEM `MQTT2_DEVICE` definition for telemetry plus the validated `setList` |

## Prerequisites

- FHEM with either `MQTT2_SERVER` or `MQTT2_CLIENT`
- A working MQTT broker connection
- Controller firmware configured with Wi-Fi and MQTT credentials
- Matching MQTT root topic between firmware and FHEM

The checked-in FHEM definition uses the verified bench root topic
`aquarium_cooling`. The committed firmware default is `aquarium/cooling`.
Adjust the topic root in the FHEM file if your `network_config.local.h` uses a
different value.

## Installation

1. Ensure the controller publishes telemetry to the broker.
2. In FHEM, create or reuse an MQTT2 IO device for the broker.
3. Paste the content of `aquarium-cooling-mqtt2-device.cfg` into FHEM.
4. Replace `MQTT2_BROKER` with the actual IODev name if needed.
5. Replace `aquarium_cooling` with your configured MQTT root topic if needed.
6. Save the FHEM configuration.

Expected readings include:

- `water_temp_c`
- `target_temp_c`
- `cooling_on_delta_c`
- `cooling_off_delta_c`
- `high_cooling_delta_c`
- `fan_low_pwm_percent`
- `fan_high_pwm_percent`
- `fan_pwm_percent`
- `fan_rpm`
- `controller_mode`
- `expected_rpm`
- `rpm_tolerance`
- `rpm_error`
- `plausibility_active`
- `fan_plausible`
- `fan_fault`
- `water_sensor_ok`
- `cooling_degraded`
- `service_required`
- `alarm_code`
- `fault_severity`
- `fault_response`
- `remote_config_last_result`
- `remote_config_last_key`
- `remote_config_last_detail`
- `remote_config_accept_count`
- `remote_config_reject_count`
- `firmware_version`
- `network_ip`
- `ota_state`
- `ota_message`
- `ota_window_active`
- `ota_upload_url`
- `availability`

Displayed temperature readings now arrive from the firmware already rounded to
one decimal place. This is only an output-formatting change; the controller
keeps full floating-point precision internally.

## Recommended DBLog Profile

For permanent long-running DBLog retention, keep the include list deliberately
small. The recommended baseline is:

- `water_temp_c`
- `target_temp_c`
- `fan_pwm_percent`
- `controller_mode`
- `fan_fault`
- `alarm_code`
- `fault_severity`
- `fault_response`
- `availability`

Detailed RPM/plausibility topics, OTA detail readings, and remote-config status
details are better enabled only temporarily for focused debug campaigns.

## Troubleshooting

If readings do not update, first compare the firmware's reported root topic
from the serial `network` command with the topic root in the FHEM
`readingList`.

If the device remains offline in FHEM while the controller works locally, check
the broker IODev, subscriptions, MQTT credentials, and whether retained
availability messages are present on `<root>/status/availability`.
