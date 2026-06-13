# FHEM MQTT2 Integration

This directory contains the FHEM definition for observing the aquarium cooling
controller and sending validated staged-control settings through MQTT.

The checked-in visual presentation is aligned with the current firmware source
version `0.1.6` and uses only SVG icons that are already shipped with standard
FHEM icon paths.

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

## Visual State Mapping

The `MQTT2_DEVICE` definition includes:

- a fixed device `icon` using a standard FHEM SVG
- a reading-driven Perl `devStateIcon`
- a compact `stateFormat` for the textual status

The icon priority is intentionally strict:

1. MQTT availability
2. OTA state while `ota_window_active=true`
3. critical fault/alarm state
4. degraded or fallback state
5. normal cooling mode

Completed or timed-out OTA end states remain available in the dedicated OTA
readings, but they no longer override the main device icon once the OTA window
is closed.

For firmware `0.1.6`, the normal-mode mapping assumes:

- `fan-off` => ready/idle
- `fan-low` => active low-stage cooling
- `fan-high` => active high-stage cooling
- `water-sensor-fallback` => fallback/warning state

Older pre-`0.1.6` controller-mode labels are intentionally not visualized in
the checked-in config.

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

The production firmware now publishes a complete MQTT telemetry snapshot every
60 seconds by default. Important controller and fault-state changes are still
published immediately, so FHEM keeps timely visibility without receiving a
full repeated snapshot every 10 seconds.

### Suggested FHEM Configuration

If your DbLog device is named `LogDB`, a practical minimal setup is:

```text
attr LogDB DbLogSelectionMode Include
attr AquariumCooling DbLogInclude water_temp_c:300,target_temp_c:300,fan_pwm_percent:300,controller_mode,fan_fault,alarm_code,fault_severity,fault_response,availability
```

This keeps the long-running database focused on:

- water temperature trend
- effective target
- commanded fan activity
- state transitions
- fault history
- MQTT availability

The `:300` suffix on the slower trend readings is a good starting point for a
120-liter tank. Changed values still get logged; unchanged values do not flood
the database. Leave `controller_mode`, `fan_fault`, `alarm_code`,
`fault_severity`, `fault_response`, and `availability` without an interval so
state changes are recorded immediately. With the firmware's 60-second baseline
publish cadence, this profile stays compact while still preserving the relevant
temperature trend for a thermally sluggish 120-liter aquarium.

For a temporary fan-debug campaign, extend the device-side include list
temporarily with:

```text
fan_rpm,expected_rpm,rpm_tolerance,rpm_error,plausibility_active,fan_plausible
```

## Legacy Topic Cleanup

If an installed controller has already gone through the older air-assist and
dual-sensor firmware generations, legacy MQTT retained topics can survive on
the broker even after the current water-only firmware is running.

Typical leftovers are:

- `air_sensor_ok`
- `air_assist_enable`
- `air_min_pwm_percent`

These are no longer part of the active firmware telemetry surface. If they
still appear in broker subscriptions or old FHEM readings, clear the retained
topics once on the broker side:

```text
mosquitto_pub -h <broker-host> -t aquarium_cooling/status/air_sensor_ok -r -n
mosquitto_pub -h <broker-host> -t aquarium_cooling/state/air_assist_enable -r -n
mosquitto_pub -h <broker-host> -t aquarium_cooling/state/air_min_pwm_percent -r -n
```

Afterwards, remove stale historical readings from the FHEM device if they are
still shown in the UI:

```text
deletereading <device-name> air_sensor_ok
deletereading <device-name> air_assist_enable
deletereading <device-name> air_min_pwm_percent
```

## Troubleshooting

If readings do not update, first compare the firmware's reported root topic
from the serial `network` command with the topic root in the FHEM
`readingList`.

If the device remains offline in FHEM while the controller works locally, check
the broker IODev, subscriptions, MQTT credentials, and whether retained
availability messages are present on `<root>/status/availability`.
