# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Added first BIN-only OTA upload implementation with a temporary ESP32-hosted
  upload endpoint and serial `ota status`, `ota enable`, and `ota cancel`
  commands.

### Changed

- Updated the OTA specification to use a manual BIN-only ESP32 upload
  maintenance mode without ZIP archives, manifests, external update polling,
  passwords, or tokens.

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
