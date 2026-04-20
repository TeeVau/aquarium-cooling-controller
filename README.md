# Aquarium Cooling Controller

![Status](https://img.shields.io/badge/status-bench--verified-brightgreen)
![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Firmware](https://img.shields.io/badge/firmware-Arduino-green)
![Control](https://img.shields.io/badge/control-local-important)

ESP32-based aquarium cooling controller for a covered tank with local autonomous fan control, shared 1-Wire DS18B20 sensing, tach-based fan plausibility monitoring, and broker-verified MQTT telemetry.

## Table of Contents

- [Overview](#overview)
- [Current Status](#current-status)
- [Features](#features)
- [Hardware](#hardware)
- [Wiring and Pin Mapping](#wiring-and-pin-mapping)
- [Repository Layout](#repository-layout)
- [Software Dependencies](#software-dependencies)
- [Build and Flash](#build-and-flash)
- [Usage](#usage)
- [Bench Results](#bench-results)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)
- [Project Links](#project-links)

## Overview

This project controls aquarium cooling with a 4-pin PWM fan and two DS18B20 sensors:

- one fixed water sensor for the main control variable
- one fixed air sensor for warm-air assist under the lid

The main design rule is that cooling must continue to work locally on the ESP32 even when Wi-Fi, MQTT, or OTA services are unavailable. The controller therefore keeps sensing, target validation, PWM generation, RPM measurement, and fault handling on the device itself.

The repository currently contains both:

- a completed fan-characterization workflow for the selected Noctua fan
- a bench-verified local controller milestone with water control, air-assist, persisted target temperature, fault policy, and MQTT telemetry

## Current Status

The project has moved beyond pure bring-up and now has a usable first controller firmware on real hardware.

Current firmware release: `0.1.0` (`2026-04-20`).

Implemented and bench-verified:

- PWM fan drive on the real output pin
- tachometer RPM measurement
- start-boost for reliable fan startup
- measured PWM-to-RPM interpolation for plausibility checks
- shared 1-Wire bus on `GPIO33`
- fixed ROM-ID assignment for water and air sensors
- local water-temperature control
- local air-assist control using `max(water_based_pwm, air_based_pwm)`
- target temperature persistence in ESP32 Preferences / NVS
- strict default and fallback target temperature of `23.0 C`
- serial diagnostics with sensor, fan, and alarm information
- verified local fault policy for water sensor, air sensor, tach, and RPM deviation failures

Implemented and broker-verified:

- non-blocking Wi-Fi connection management
- MQTT telemetry publishing with `PubSubClient`
- local secret override via ignored `network_config.local.h`
- broker-side normal telemetry capture
- broker-side fault telemetry for air sensor, water sensor, and fan-fault cases

Still intentionally open:

- real-aquarium tuning of water and air control behavior
- longer real-aquarium live data capture after the first 2 h installed run
- MQTT remote configuration
- OTA update path

## Features

Implemented now:

- Local autonomous control on ESP32 without network dependency
- Fan characterization sketch for the selected 4-pin PWM fan
- Measured fan curve reused in production firmware
- Water-temperature based control with default target `23.0 C`
- Air-assist based on lid air temperature
- Fixed DS18B20 role mapping by ROM ID instead of bus order
- Target temperature persistence across reboot
- Safe fallback to `23.0 C` for invalid or missing target values
- Tach plausibility diagnostics against the measured fan curve
- Central fault-policy model with alarm severity and response labels
- Hardware-verified fault responses for sensor failures and fan plausibility faults
- Wi-Fi/MQTT telemetry that does not block local cooling
- Serial service commands for diagnostics and bench operation

Planned next:

- longer aquarium-side live data capture
- aquarium-side control tuning with recorded live operating data
- MQTT remote parameter updates
- OTA firmware updates over Wi-Fi

## Hardware

Bench hardware used so far:

| Component | Quantity | Purpose | Notes |
|---|---:|---|---|
| ESP32 Dev Board | 1 | Main controller | Bench-tested with `esp32:esp32:esp32` |
| Noctua NF-S12A PWM | 1 | Cooling actuator | 120 mm 4-pin PWM fan |
| DS18B20 | 2 | Water and air temperature sensing | Shared 1-Wire bus |
| 12 V supply | 1 | Fan power | Common ground with ESP32 is mandatory |
| `3.3 kOhm` pull-up | 1 | Tach input biasing | To `3.3 V` |
| `3.3 kOhm` pull-up | 1 | 1-Wire bus biasing | To `3.3 V` |

Planned production hardware concept:

| Component | Role |
|---|---|
| ESP32-WROOM-32E / DevKitC-class board | Main controller |
| USB-C PD trigger board | Single-cable 12 V input |
| 12 V to 5 V buck converter | ESP32 supply |
| 3D-printed enclosure | Centralized electronics housing |
| Terminal blocks | Fan and sensor wiring termination |

## Wiring and Pin Mapping

Current bench mapping:

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| Fan PWM | 25 | 25 kHz PWM output |
| Fan TACH | 26 | Tach input with `3.3 kOhm` pull-up |
| 1-Wire bus | 33 | Shared DS18B20 bus with `3.3 kOhm` pull-up |

Fan connector reference:

| Fan Pin | Signal | Typical Color | Project Connection |
|---|---|---|---|
| 1 | GND | Black | Common GND |
| 2 | +12 V | Yellow | 12 V fan supply |
| 3 | TACH | Green | ESP32 tach input |
| 4 | PWM | Blue | ESP32 PWM output stage |

Verified DS18B20 cable colors for the currently used potted sensors:

| Sensor Wire | Function |
|---|---|
| `rot` | `3.3 V` |
| `gelb` | `GND` |
| `gruen` | `DATA` |

Important notes:

- `ESP32 GND`, fan ground, and the `12 V` supply ground must share a common reference.
- Water sensor ROM: `28333844050000CB`
- Air sensor ROM: `28244644050000DA`
- A block-level wiring sketch is available in [docs/design/schematic-sketch.md](docs/design/schematic-sketch.md).

## Repository Layout

```text
.
|- README.md
|- docs/
|  |- aquarium-cooling-controller-fsd.md
|  |- sensor-bringup-2026-04-12.md
|  |- design/
|  `- result fan test/
|- firmware/
|  |- controller/
|  `- fan-test/
|- integrations/
|  `- fhem/
`- tools/
```

Key files:

- FSD: [docs/aquarium-cooling-controller-fsd.md](docs/aquarium-cooling-controller-fsd.md)
- Sensor bring-up notes: [docs/sensor-bringup-2026-04-12.md](docs/sensor-bringup-2026-04-12.md)
- Fan measurement summary: [docs/result fan test/measurement-summary-2026-04-12.md](docs/result%20fan%20test/measurement-summary-2026-04-12.md)
- Controller firmware: [firmware/controller/controller.ino](firmware/controller/controller.ino)
- Controller diagrams: [docs/design/controller-diagrams.md](docs/design/controller-diagrams.md)
- Mermaid rendering notes: [docs/design/mermaid-rendering.md](docs/design/mermaid-rendering.md)
- Fan characterization sketch: [firmware/fan-test/fan-test.ino](firmware/fan-test/fan-test.ino)
- FHEM MQTT2 integration notes: [integrations/fhem/README.md](integrations/fhem/README.md)
- FHEM MQTT2 device definition: [integrations/fhem/aquarium-cooling-mqtt2-device.cfg](integrations/fhem/aquarium-cooling-mqtt2-device.cfg)
- Serial capture helper: [tools/serial-capture.ps1](tools/serial-capture.ps1)
- MQTT client helper: [tools/mqtt-client.ps1](tools/mqtt-client.ps1)
- MQTT telemetry notes: [docs/mqtt-telemetry-2026-04-16.md](docs/mqtt-telemetry-2026-04-16.md)

## Software Dependencies

Required tools:

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x or compatible `arduino-cli`
- ESP32 board package for Arduino
- PowerShell on Windows for the serial and MQTT helper scripts
- Mosquitto command-line clients for broker-side MQTT verification

Required Arduino libraries:

| Library | Purpose | Install via |
|---|---|---|
| `OneWire` | Shared DS18B20 bus transport | Arduino Library Manager |
| `DallasTemperature` | DS18B20 sensor handling | Arduino Library Manager |
| `PubSubClient` | MQTT client for ESP32 telemetry | Arduino Library Manager |

Recommended ESP32 board package URL:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Observed in the latest verified bench run:

- FQBN: `esp32:esp32:esp32`
- ESP32 Arduino core reported at runtime: `3.3.8`

## Build and Flash

### Arduino IDE

1. Install Arduino IDE 2.x.
2. Add the ESP32 board package URL in `File -> Preferences`.
3. Install the `esp32` board package in Boards Manager.
4. Install `OneWire`, `DallasTemperature`, and `PubSubClient` in the Library Manager.
5. Open one of these sketches:
   - `firmware/fan-test/fan-test.ino`
   - `firmware/controller/controller.ino`

### Arduino CLI

Compile the controller:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32 --build-path '.arduino-build\esp32_esp32_esp32' --output-dir 'build' 'firmware/controller'
```

Upload the controller:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' upload -p COM3 --fqbn esp32:esp32:esp32 --build-path '.arduino-build\esp32_esp32_esp32' 'firmware/controller'
```

Capture serial output:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial-capture.ps1 -Port COM3 -Baud 115200 -DurationSec 15 -OutputPath 'build\serial-log.txt'
```

Subscribe to MQTT telemetry with the helper script:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\mqtt-client.ps1 -Mode sub -BrokerHost <broker-host> -RootTopic aquarium_cooling -Count 45
```

## Usage

### 1. Fan Characterization

Use [firmware/fan-test/fan-test.ino](firmware/fan-test/fan-test.ino) to:

- detect start PWM from standstill
- detect minimum hold PWM
- measure the upward and downward PWM-to-RPM curves
- print reusable curve data for the production controller

### 2. Local Controller

Use [firmware/controller/controller.ino](firmware/controller/controller.ino) for the current controller firmware.

What it does today:

- initializes PWM output, tach measurement, and the 1-Wire bus
- assigns water and air sensors by fixed ROM ID
- computes local water-based fan demand
- computes air-assist fan demand from the air sensor
- selects `final_pwm = max(water_based_pwm, air_based_pwm)`
- persists a user-set target temperature in Preferences / NVS
- falls back to the default target `23.0 C` if the stored or entered target is invalid
- prints continuous diagnostics including target source, alarm code, sensor health, RPM plausibility, and assigned ROM IDs

Serial monitor:

```text
115200 baud
```

Current serial service commands:

| Command | Effect |
|---|---|
| `status` | Print diagnostics immediately |
| `target <c>` | Set and persist a custom target temperature |
| `default` | Clear the stored target and return to `23.0 C` |
| `airassist` | Print the current air-assist defaults |
| `faults` | Print the current fault-policy defaults |
| `network` | Print Wi-Fi/MQTT configuration and connection status |
| `publish` | Publish telemetry immediately when MQTT is connected |
| `help` | Show the command list |

Example session:

```text
target 24.5
status
default
```

### 3. MQTT Telemetry

MQTT is optional and non-critical. If Wi-Fi or MQTT is unavailable, local cooling, fan control, sensor handling, and fault policy continue to run on the ESP32.

Committed defaults intentionally contain no secrets. To enable telemetry locally:

1. Copy [network_config.local.example.h](firmware/controller/network_config.local.example.h) to `firmware/controller/network_config.local.h`.
2. Fill in `AQ_WIFI_SSID`, `AQ_WIFI_PASSWORD`, `AQ_MQTT_HOST`, and optional MQTT credentials.
3. Compile and flash the controller firmware.
4. Use the serial command `network` to inspect connection state.
5. Use `publish` to force a telemetry publish after the first diagnostics cycle.

`network_config.local.h` is ignored by Git and must not be committed.

Published topics use the root `aquarium/cooling` by default. The current local bench setup has also been verified with the configured root `aquarium_cooling`.

| Topic suffix | Payload |
|---|---|
| `/state/water_temp_c` | water temperature or `unavailable` |
| `/state/air_temp_c` | air temperature or `unavailable` |
| `/state/target_temp_c` | active target temperature |
| `/state/fan_pwm_percent` | final commanded fan PWM |
| `/state/fan_rpm` | measured fan RPM |
| `/state/controller_mode` | local control mode |
| `/diagnostic/expected_rpm` | interpolated expected RPM |
| `/diagnostic/rpm_tolerance` | current RPM tolerance |
| `/diagnostic/rpm_error` | measured minus expected RPM |
| `/diagnostic/plausibility_active` | whether RPM plausibility is currently active |
| `/status/fan_plausible` | current plausibility result |
| `/status/fan_fault` | latched fan fault |
| `/status/water_sensor_ok` | water sensor health |
| `/status/air_sensor_ok` | air sensor health |
| `/status/alarm_code` | summarized fault code |
| `/status/fault_severity` | `none`, `warning`, or `critical` |
| `/status/fault_response` | local fault response |
| `/status/cooling_degraded` | whether cooling effectiveness is degraded |
| `/status/service_required` | whether service/operator action is required |
| `/status/availability` | MQTT last-will availability |

### 4. FHEM MQTT2 Integration

The repository includes a ready-to-paste FHEM `MQTT2_DEVICE` definition for
the current telemetry topics:

```text
integrations/fhem/aquarium-cooling-mqtt2-device.cfg
```

The FHEM definition is intended for monitoring. It creates readings for the
published state, diagnostics, fault-policy values, and MQTT availability. The
current firmware does not yet subscribe to remote `/set/...` topics, so the
future FHEM `setList` is kept commented in the configuration file.

The checked-in FHEM file uses the verified bench root topic `aquarium_cooling`.
If your firmware uses the committed default root `aquarium/cooling`, replace
`aquarium_cooling` in the FHEM file before importing it.

## Bench Results

Verified fan characterization for the current Noctua bench setup:

- start PWM from standstill: `12%`
- minimum hold PWM while spinning: `10%`
- `0%` PWM: `0 RPM`
- `5%` PWM: `0 RPM`
- `100%` PWM: `1252 RPM`

Verified controller milestone on hardware:

- compile and flash successful
- fan driver initialization successful
- RPM monitor initialization successful
- both DS18B20 sensors detected on the shared 1-Wire bus
- water and air sensor ROMs matched correctly
- default target temperature verified at `23.0 C`
- invalid target input falls back to `23.0 C`
- persisted target survives reboot
- local water control behaves plausibly
- first air-assist behavior behaves plausibly
- fault policy reports alarm code, severity, response, service requirement, and degraded-cooling state
- water-sensor failure enters `water-sensor-fault`, `critical`, and `water-fallback` at `40%` PWM
- air-sensor failure enters `air-sensor-fault`, `warning`, and `disable-air-assist`
- missing tach feedback enters `fan-fault`, `critical`, and `report-fan-fault`
- slowed fan / RPM deviation outside tolerance enters `fan-fault` after the configured mismatch debounce
- fan fault recovery returns to `none` after the configured plausible-match debounce
- Wi-Fi connects and reports an assigned IP through the `network` command
- MQTT connects to the broker and publishes with the configured root topic
- normal broker capture observed all 20 expected state, diagnostic, and status topics
- broker telemetry reports plausible normal values such as `water-control`, target `23.00`, fan RPM, expected RPM, tolerance, and RPM error
- air-sensor fault publishes `air-sensor-fault`, `warning`, and `disable-air-assist`
- water-sensor fault publishes `water-sensor-fault`, `critical`, `water-fallback`, `cooling_degraded=true`, and fallback fan PWM
- fan RPM deviation publishes `fan-fault`, `critical`, `report-fan-fault`, `cooling_degraded=true`, and `service_required=true`
- broker telemetry returns to `alarm_code=none` after fault recovery

Useful artifacts:

- [docs/sensor-bringup-2026-04-12.md](docs/sensor-bringup-2026-04-12.md)
- [docs/fault-policy-2026-04-16.md](docs/fault-policy-2026-04-16.md)
- [docs/mqtt-telemetry-2026-04-16.md](docs/mqtt-telemetry-2026-04-16.md)
- [docs/aquarium-live-tests/2026-04-16-aquarium-2h-summary.md](docs/aquarium-live-tests/2026-04-16-aquarium-2h-summary.md)
- [docs/aquarium-live-tests/2026-04-16-aquarium-2h-fhem-export.csv](docs/aquarium-live-tests/2026-04-16-aquarium-2h-fhem-export.csv)
- [integrations/fhem/README.md](integrations/fhem/README.md)
- [docs/result fan test/fan-curve-chart.svg](docs/result%20fan%20test/fan-curve-chart.svg)
- [docs/result fan test/controller-smoke-test-2026-04-12.md](docs/result%20fan%20test/controller-smoke-test-2026-04-12.md)

## Troubleshooting

### Fan does not react to PWM

- Verify common ground between ESP32, fan, and `12 V` supply.
- Check that PWM is wired to the real fan PWM pin.
- Confirm the intended firmware was actually flashed.

### RPM stays at zero

- Check the tach pull-up to `3.3 V`.
- Verify tach is wired to `GPIO26`.
- Confirm the fan model really provides tach pulses.

### DS18B20 sensors are not detected

- Verify the 1-Wire bus is on `GPIO33`.
- Check the `3.3 kOhm` pull-up from data to `3.3 V`.
- Confirm shared ground with the ESP32.
- Re-check the verified potted-sensor colors: `rot = 3.3 V`, `gelb = GND`, `gruen = DATA`.

### Target temperature behaves unexpectedly

- Run `status` and inspect `Target source` and `Target defaulted`.
- Use `default` to clear persisted target temperature and return to `23.0 C`.
- If `target <c>` is outside the valid range, the firmware intentionally falls back to `23.0 C`.

### FHEM readings do not update

- Verify that the FHEM `IODev` points to the correct `MQTT2_CLIENT` or `MQTT2_SERVER`.
- Compare the root topic reported by the serial `network` command with the root topic in [integrations/fhem/aquarium-cooling-mqtt2-device.cfg](integrations/fhem/aquarium-cooling-mqtt2-device.cfg).
- Confirm the broker receives `<root>/status/availability` as `online`.
- Remember that FHEM set commands are intentionally inactive until MQTT remote configuration is implemented in firmware.

### Upload fails

- Re-check the selected COM port.
- Close any open serial monitor that still holds the port.
- Verify Arduino IDE and `arduino-cli` are using the same ESP32 core installation.

## Roadmap

Next likely steps:

1. Extend the first installed aquarium capture to longer live data for water and air control tuning.
2. Add MQTT remote parameter updates with validation and persistence.
3. Add OTA support over Wi-Fi.
4. Finalize enclosure, wiring, and installation layout for the real aquarium.
5. Re-check fan plausibility in the final installed airflow path.

## Contributing

Issues, ideas, measurements, and implementation feedback are welcome.

If you want to contribute code:

1. Open an issue or describe the intended change first.
2. Keep hardware assumptions explicit.
3. Preserve the rule that critical cooling stays local on the ESP32.
4. Test on real hardware when a change touches PWM, tach, or sensor handling.

## License

This repository is licensed under the GNU General Public License v3.0.

See [LICENSE](LICENSE) for the full license text.

## Project Links

- Repository: [github.com/TeeVau/aquarium-cooling-controller](https://github.com/TeeVau/aquarium-cooling-controller)
- Functional specification: [docs/aquarium-cooling-controller-fsd.md](docs/aquarium-cooling-controller-fsd.md)
