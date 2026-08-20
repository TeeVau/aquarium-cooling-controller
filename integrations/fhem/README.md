# FHEM MQTT2 Integration

This directory contains a pasteable FHEM `MQTT2_DEVICE` definition for the
Aquarium Cooling Controller.

The checked-in configuration is based on the current running installation:

- FHEM device name: `wz_AquariumCooling`
- MQTT client / server IODev: `myBroker`
- Room: `Wohnzimmer`
- Topic root: `aquarium_cooling`
- Release: `0.3.0`

The committed template keeps portable placeholder names such as
`AquariumCooling` and `MQTT2_BROKER`; replace them with your local device names
before importing if needed.

## Files

| File | Purpose |
|---|---|
| `aquarium-cooling-mqtt2-device.cfg` | Pasteable FHEM `MQTT2_DEVICE` template with readings, status display, DBLog profile, and validated set commands |

## Installation

1. Ensure the controller publishes MQTT telemetry.
2. Create or reuse an FHEM `MQTT2_CLIENT` or `MQTT2_SERVER`.
3. Import `aquarium-cooling-mqtt2-device.cfg`.
4. Replace `MQTT2_BROKER` with your FHEM MQTT IODev name.
5. Replace `aquarium_cooling` if your firmware uses another MQTT root topic.
6. Adjust `room`, `group`, and device name to your FHEM setup.
7. Save the FHEM configuration.

## Readings

The template maps the controller's MQTT state, diagnostic, status, OTA, and
remote-configuration topics to FHEM readings.

The optional OLED is intentionally local-only in `0.3.0`; it does not add a
separate MQTT topic surface and has no impact on the FHEM integration when it
is missing or disabled.

Core readings for normal operation:

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
- `fan_plausible`
- `fan_fault`
- `water_sensor_ok`
- `cooling_degraded`
- `service_required`
- `alarm_code`
- `fault_severity`
- `fault_response`
- `firmware_version`
- `network_ip`
- `availability`

Maintenance and configuration readings:

- `ota_state`
- `ota_message`
- `ota_window_active`
- `ota_upload_url`
- `remote_config_last_result`
- `remote_config_last_key`
- `remote_config_last_detail`
- `remote_config_accept_count`
- `remote_config_reject_count`

Detailed RPM plausibility readings are included for visibility but are not part
of the default DBLog include list:

- `expected_rpm`
- `rpm_tolerance`
- `rpm_error`
- `plausibility_active`

## Status Display

The template sets both `stateFormat` and `devStateIcon`.

Priority order:

1. MQTT offline state.
2. Active OTA upload window.
3. Critical alarm, fan fault, or water-sensor fault.
4. Degraded or fallback cooling state.
5. Normal cooling mode.

The visible state line follows this shape:

```text
Kuehlt | Wasser 24.1 C | Ziel 24.0 C | Luefter 45 % | MQTT online
```

The template uses standard FHEM icons only.

## Set Commands

FHEM can send validated MQTT settings to the controller:

| Set command | Meaning |
|---|---|
| `target_temp_c` | Target water temperature |
| `cooling_on_delta_c` | Delta above target that starts cooling |
| `cooling_off_delta_c` | Delta below target that stops cooling |
| `high_cooling_delta_c` | Delta above target that selects `fan-high` |
| `fan_low_pwm_percent` | Fixed PWM for `fan-low` |
| `fan_high_pwm_percent` | Fixed PWM for `fan-high` |
| `ota_enable` | `true` opens, `false` cancels the OTA window |

Local control on the ESP32 remains authoritative. Invalid values are rejected
by the firmware and reported through the `remote_config_*` readings.

## DBLog Profile

The field-tested profile for the running 120-liter aquarium is:

```text
attr AquariumCooling event-on-change-reading .*
attr AquariumCooling event-min-interval .*:1800
attr AquariumCooling DbLogInclude water_temp_c:1800,target_temp_c:86400,fan_pwm_percent:300,controller_mode,fan_fault,alarm_code,fault_severity,fault_response,availability
```

This keeps long-running database growth under control while retaining the core
signals needed to understand water temperature, target, fan activity, operating
mode, fault state, and MQTT availability.

For temporary fan diagnostics, extend the DBLog include list with:

```text
fan_rpm,expected_rpm,rpm_tolerance,rpm_error,plausibility_active,fan_plausible
```

Remove those detailed readings again after the diagnostic window to keep the
long-term history compact.

## Topic Root

The template uses:

```text
aquarium_cooling
```

The firmware default is:

```text
aquarium/cooling
```

Use the serial `network` command to check which root topic your controller is
publishing, then keep the firmware and FHEM `readingList` / `setList` aligned.

## Troubleshooting

If readings do not update:

- Check that `IODev` points to the active FHEM MQTT device.
- Compare the serial `network` topic root with the FHEM topic root.
- Confirm `<root>/status/availability` is published as `online`.
- Confirm broker credentials and subscriptions.

If set commands do not apply:

- Check `remote_config_last_result`.
- Check `remote_config_last_key`.
- Check `remote_config_last_detail`.
- Verify the value is inside the firmware validation range.

If OTA does not start from FHEM:

- Set `ota_enable` to `true`.
- Watch `ota_window_active`, `ota_state`, and `ota_upload_url`.
- Prefer the repository OTA script for actual firmware uploads:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\ota-upload.ps1
```
