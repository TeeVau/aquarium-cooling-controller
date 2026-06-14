# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.2.0] - 2026-06-14

### Added

- Added staged water-only cooling with `fan-off`, `fan-low`, `fan-high`, and
  `water-sensor-fallback` controller modes.
- Added persisted MQTT remote configuration for target temperature, hysteresis
  deltas, low/high fan stages, and OTA enablement.
- Added MQTT-visible OTA status, controller IP, upload URL, remote-config
  result readings, and firmware version reporting.
- Added `tools/ota-upload.ps1` as the standard OTA flashing workflow for the
  installed controller.
- Added a field-tested FHEM `MQTT2_DEVICE` profile with status display,
  set commands, DBLog include list, and long-running event throttling.

### Changed

- Promoted the firmware version to `0.2.0`.
- Made water temperature the active control variable and removed the active
  air-sensor control path from the current firmware.
- Set the default staged fan levels to `45%` for `fan-low` and `60%` for
  `fan-high`.
- Changed MQTT full telemetry snapshots to a 60-second baseline with immediate
  publishes for relevant controller, fault, and accepted remote-config changes.
- Reworked the README into a user-facing product guide and moved historical
  development detail out of the main project path.
- Updated FHEM documentation to use the current live profile:
  `event-on-change-reading .*`, `event-min-interval .*:1800`, and
  `DbLogInclude water_temp_c:1800,target_temp_c:86400,fan_pwm_percent:300,controller_mode,fan_fault,alarm_code,fault_severity,fault_response,availability`.
- Standardized repository build and maintenance workflows on
  `tools/build.ps1`, `tools/mqtt-client.ps1`, and `tools/ota-upload.ps1`.

## [0.1.4] - 2026-05-01

### Added

- Added BIN-only OTA uploads through a temporary ESP32-hosted upload endpoint
  with serial `ota status`, `ota enable`, and `ota cancel` commands.
- Added explicit firmware identity and firmware-version tags inside the OTA
  image so OTA validation does not depend on Arduino `PROJECT_NAME` or ESP32
  build metadata.
- Added validated MQTT remote configuration with persistence and broker-visible
  feedback for accepted and rejected commands.
- Added retained MQTT status topics for `network_ip` and `ota_upload_url` so
  OTA can be discovered and targeted entirely through the broker-facing
  workflow.

### Changed

- Added MQTT control of the temporary OTA maintenance window and published OTA
  state/message topics plus the running firmware version for FHEM and
  broker-side observability.
- Updated the OTA specification to use a manual BIN-only ESP32 upload
  maintenance mode without ZIP archives, manifests, external update polling,
  passwords, or tokens.
- Hardened BIN-only OTA validation to reject incomplete uploads, confirm the
  freshly booted OTA image for rollback handling, and validate uploads against
  the intended firmware identity plus a newer SemVer release.
- Simplified steady-state control to water-only hysteresis behavior and removed
  the active auxiliary-air control path from the installed controller firmware.
- Documented the repository's canonical build artifact layout:
  `.arduino-build/` for working compilation state, `build/` for exported
  binaries and logs, and `firmware/controller/build/` as a disposable Arduino
  tooling artifact directory.
- Confirmed the BIN-only OTA workflow on ESP32 hardware, including MQTT
  enablement, upload endpoint publication, validation failure handling, reboot,
  and firmware `0.1.4` reporting.
- Rounded displayed and published temperature values to one decimal place at
  output boundaries while keeping internal control values at full floating-point
  precision.
- Updated the FHEM integration and project documentation for the new MQTT
  remote configuration surface and the one-decimal display convention.

## [0.1.0] - 2026-04-20

### Added

- Added the ESP32 aquarium cooling controller firmware baseline.
- Added local autonomous water-temperature control with DS18B20 sensing.
- Added fixed DS18B20 role mapping by ROM ID.
- 4-pin PWM fan control with start boost and measured fan-curve data.
- Tachometer RPM measurement and fan plausibility diagnostics.
- Fault policy for water-sensor and fan-plausibility faults.
- Preferences/NVS persistence for custom target temperature.
- Non-blocking Wi-Fi and publish-only MQTT telemetry.
- FHEM MQTT2 monitoring definition for the verified telemetry topic set.
- Project FSD, design diagrams, fan characterization notes, and field-test
  documentation.
- Changelog structure and documented SemVer / Keep a Changelog release
  policy.

[Unreleased]: https://github.com/TeeVau/aquarium-cooling-controller/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/TeeVau/aquarium-cooling-controller/releases/tag/v0.2.0
[0.1.4]: https://github.com/TeeVau/aquarium-cooling-controller/releases/tag/v0.1.4
[0.1.0]: https://github.com/TeeVau/aquarium-cooling-controller/releases/tag/v0.1.0
