# Aquarium Cooling Controller API

This generated documentation covers the ESP32 firmware interfaces for aquarium
cooling control, fan PWM output, fan RPM monitoring, OneWire temperature
sampling, MQTT telemetry, OTA upload handling, and fault policy decisions.

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
- OTA upload server: temporary browser-based firmware upload window.
- Controller sketch: Arduino setup/loop orchestration, serial diagnostics, target
  persistence, and module wiring.

## Generated Output

The GitHub Pages workflow builds this site from `Doxyfile` and publishes the
HTML output from `docs/doxygen/html`.
