# FHEM MQTT2 Integration

This directory contains the FHEM definition for observing the aquarium cooling
controller through MQTT.

## Current Scope

The firmware state documented on 2026-04-16 is publish-only MQTT telemetry.
FHEM can therefore display the controller state, diagnostics, availability, and
fault-policy readings, but it should not be treated as the active control plane.
Local cooling on the ESP32 remains authoritative.

Remote `/set/...` topics are planned in the project specification, but the
current firmware does not subscribe to them yet. The corresponding FHEM
`setList` is included in the configuration file as a commented future block.

## Files

| File | Purpose |
|---|---|
| `aquarium-cooling-mqtt2-device.cfg` | Pasteable FHEM `MQTT2_DEVICE` definition for all currently published telemetry topics |

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
- `air_temp_c`
- `target_temp_c`
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
- `air_sensor_ok`
- `cooling_degraded`
- `service_required`
- `alarm_code`
- `fault_severity`
- `fault_response`
- `availability`

## Troubleshooting

If readings do not update, first compare the firmware's reported root topic
from the serial `network` command with the topic root in the FHEM
`readingList`.

If the device remains offline in FHEM while the controller works locally, check
the broker IODev, subscriptions, MQTT credentials, and whether retained
availability messages are present on `<root>/status/availability`.
