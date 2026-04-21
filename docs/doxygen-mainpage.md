# Aquarium Cooling Controller API

This generated documentation covers the ESP32 firmware interfaces for aquarium
cooling control, fan PWM output, fan RPM monitoring, OneWire temperature
sampling, MQTT telemetry, serial diagnostics, and fault policy decisions.

Use the project README for setup, wiring, build, and operating instructions.
This Doxygen site focuses on source-level API documentation.

## Firmware Areas

- Control engine: target validation and PWM command calculation.
- Fan curve: measured PWM-to-RPM mapping and plausibility helpers.
- Fan driver: ESP32 LEDC PWM output with low-speed start boost.
- RPM monitor: interrupt-driven tachometer pulse counting.
- Fault monitor and policy: fan plausibility, alarm state, severity, and response.
- Sensor manager: OneWire discovery and DS18B20 temperature sampling.
- MQTT telemetry: network status and controller-state publishing.
- Controller sketch: Arduino setup/loop orchestration, serial diagnostics, target
  persistence, and module wiring.

## Circuit Summary

- ESP32 board running the Arduino core.
- DS18B20 water sensor on the shared OneWire bus.
- DS18B20 air sensor on the same OneWire bus for warm-air assist.
- Four-wire PWM fan with separate PWM command and tachometer feedback.
- MQTT telemetry is optional; local cooling continues without Wi-Fi or broker access.

## Arduino Runtime

The firmware follows the standard Arduino lifecycle:

- `setup()` initializes serial diagnostics, preferences, sensors, fan PWM, RPM
  sampling, and telemetry.
- `loop()` remains non-blocking and repeatedly advances sensor sampling, control
  calculation, PWM output, RPM measurement, fault evaluation, diagnostics, and
  telemetry publishing.

## Documentation Policy

This site is generated with `WARN_AS_ERROR = YES`, so Doxygen warnings fail the
GitHub Pages build. New firmware modules should include `@file` documentation,
explicit API comments, and Arduino-specific notes for pins, timing, units, and
runtime assumptions.

## Generated Output

The GitHub Pages workflow builds this site from `Doxyfile` and publishes the
HTML output from `docs/doxygen/html`.
