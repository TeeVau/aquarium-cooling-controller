# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Added first BIN-only OTA upload implementation with a temporary ESP32-hosted
  upload endpoint and serial `ota status`, `ota enable`, and `ota cancel`
  commands.
- Added explicit firmware identity and firmware-version tags inside the OTA
  image so OTA validation does not depend on Arduino `PROJECT_NAME` or ESP32
  build metadata.
- Added validated MQTT remote configuration for `target_temp_c`,
  `air_assist_enable`, and `air_min_pwm_percent`, including persistence and
  MQTT/diagnostic feedback for accepted and rejected commands.

### Changed

- Updated the OTA specification to use a manual BIN-only ESP32 upload
  maintenance mode without ZIP archives, manifests, external update polling,
  passwords, or tokens.
- Hardened BIN-only OTA validation to reject incomplete uploads, confirm the
  freshly booted OTA image for rollback handling, and validate uploads against
  the intended firmware identity plus a newer SemVer release.
- Documented the repository's canonical build artifact layout:
  `.arduino-build/` for working compilation state, `build/` for exported
  binaries and bench logs, and `firmware/controller/build/` as a disposable
  Arduino tooling artifact directory.
- Verified the BIN-only OTA workflow on a bare ESP32 bench target, including
  manual enable, live HTTP upload, validation failure paths, and a successful
  OTA upgrade with reboot into the new firmware.
- Rounded displayed and published temperature values to one decimal place at
  output boundaries while keeping internal control values at full floating-point
  precision.
- Updated the FHEM integration and project documentation for the new MQTT
  remote configuration surface and the one-decimal display convention.

## [0.1.0] - 2026-04-20

### Added

- Initial bench-verified ESP32 aquarium cooling controller firmware.
- Local autonomous water-temperature control with DS18B20 water and air sensors.
- Fixed DS18B20 role mapping by ROM ID for water and air sensors.
- 4-pin PWM fan control with start boost and measured fan-curve data.
- Tachometer RPM measurement and fan plausibility diagnostics.
- Fault policy for water-sensor, air-sensor, and fan-plausibility faults.
- Preferences/NVS persistence for custom target temperature.
- Non-blocking Wi-Fi and publish-only MQTT telemetry.
- FHEM MQTT2 monitoring definition for the verified telemetry topic set.
- Project FSD, design diagrams, fan characterization notes, and live-test
  documentation.
- Initial changelog structure and documented SemVer / Keep a Changelog release
  policy.

[Unreleased]: https://github.com/TeeVau/aquarium-cooling-controller/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/TeeVau/aquarium-cooling-controller/releases/tag/v0.1.0
