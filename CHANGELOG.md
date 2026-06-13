# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Added staged water-only control with `fan-low` / `fan-high` states and
  remote-configurable hysteresis deltas plus fixed PWM stages.
- Added validated MQTT remote configuration for
  `cooling_on_delta_c`, `cooling_off_delta_c`, `high_cooling_delta_c`,
  `fan_low_pwm_percent`, and `fan_high_pwm_percent`, including persistence and
  active-state publication.
- Added concrete FHEM DBLog guidance for a minimal long-running production
  profile.
- Added archived live-test exports and summaries for the first `18%` and later
  `22%` water-only controller generations.
- Added an archived 30-day staged-control tuning export that documents the
  transition from `22 / 35` defaults to the later `45 / 60` field baseline.
- Added `tools/ota-upload.ps1` as the repository-standard OTA flashing
  entrypoint for MQTT-triggered maintenance-window enable, upload-URL
  discovery, HTTP BIN upload, and post-reboot MQTT verification.

### Changed

- Removed the active air-sensor control path from the current firmware,
  simplified the tracked sensor model to a fixed water sensor, and reduced the
  fault model accordingly.
- Increased the default MQTT full-snapshot interval from `10 s` to `60 s` for
  the 120-liter production aquarium and added immediate extra publishes for
  controller/fault state changes and accepted remote configuration updates.
- Documented the conservative field-tested FHEM history profile using targeted
  `event-min-interval` only on slow trend readings plus the matching
  `DbLogInclude` selection.
- Updated README, FSD, FHEM integration docs, and design diagrams to the staged
  water-only architecture.
- Improved the FHEM MQTT2 device presentation for firmware `0.1.6` with a
  reading-driven `devStateIcon`, a compact status line, and documented use of
  built-in FHEM SVG icons without custom assets.
- Promoted the observed field-tuned staged cooling levels from `22 / 35` to
  `45 / 60` as the checked-in source defaults for firmware `0.1.7`.
- Standardized the repository build and OTA workflow on `tools/build.ps1`,
  `tools/mqtt-client.ps1`, and `tools/ota-upload.ps1`, including board-scoped
  `.arduino-build/` logs/output, versioned `bin/` firmware exports, and
  MQTT-driven OTA validation against the live controller.
- Advanced the checked-in production firmware version to `0.1.9` and verified
  the script-driven OTA upgrade path end to end on the fully wired controller
  hardware.

## [0.1.4] - 2026-05-01

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
  the active air-assist control path from the installed controller firmware.
- Documented the repository's canonical build artifact layout:
  `.arduino-build/` for working compilation state, `build/` for exported
  binaries and bench logs, and `firmware/controller/build/` as a disposable
  Arduino tooling artifact directory.
- Verified the BIN-only OTA workflow on a bare ESP32 bench target, including
  manual enable, live HTTP upload, validation failure paths, and a successful
  OTA upgrade with reboot into the new firmware.
- Verified OTA again on the fully wired aquarium controller hardware, including
  MQTT-triggered OTA enable, MQTT publication of the active upload endpoint,
  and a successful HTTP BIN upload to firmware `0.1.4`.
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

[Unreleased]: https://github.com/TeeVau/aquarium-cooling-controller/compare/v0.1.4...HEAD
[0.1.4]: https://github.com/TeeVau/aquarium-cooling-controller/releases/tag/v0.1.4
[0.1.0]: https://github.com/TeeVau/aquarium-cooling-controller/releases/tag/v0.1.0
