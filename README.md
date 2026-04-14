# Aquarium Cooling Controller

![Status](https://img.shields.io/badge/status-experimental-orange)
![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Firmware](https://img.shields.io/badge/firmware-Arduino-green)

ESP32-based aquarium cooling controller for a covered tank with local autonomous fan control, measured PWM-to-RPM characterization, tach-based plausibility monitoring, DS18B20 water and air sensing, and planned MQTT and OTA support.

## Table of Contents

- [Overview](#overview)
- [Current Status](#current-status)
- [Features](#features)
- [Hardware](#hardware)
- [Pin Mapping](#pin-mapping)
- [Repository Layout](#repository-layout)
- [Software Dependencies](#software-dependencies)
- [Installation](#installation)
- [Usage](#usage)
- [Bench Results](#bench-results)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)
- [Project Links](#project-links)

## Overview

This project is building a compact cooling controller for an aquarium with a lid-mounted PWM fan and two DS18B20 temperature sensors. The core design goal is to keep cooling logic local on the ESP32 so the system remains useful even when Wi-Fi, MQTT, or update infrastructure is unavailable.

The repo currently covers both the characterization phase and the first production-controller bring-up phase. The selected fan has already been measured on real hardware, and the measured curve is now reused in the current controller firmware for expected RPM interpolation and plausibility diagnostics.

## Current Status

The project is in the first usable production-firmware stage.

Already done:

- Characterized the selected 4-pin PWM fan on an ESP32 bench setup
- Measured usable start, hold, and full-scale RPM points
- Added a reusable fan-curve module with interpolation
- Added a local controller with:
  - PWM output on the real fan pin
  - tachometer RPM measurement
  - start-boost behavior
  - plausibility diagnostics against the measured curve
  - DS18B20 bus bring-up with fixed ROM-ID assignment for water and air sensors
  - local water-temperature control
  - local air-assist control via `max(water, air)`
  - persisted target temperature in ESP32 Preferences/NVS
  - default and error fallback target of `23.0 C`

Current focus:

- Refine the remaining fault-reaction policy on real hardware
- Tune control behavior later with live aquarium data
- Prepare the path for MQTT telemetry and OTA updates

## Features

Implemented now:

- Real fan characterization sketch for a 4-pin PWM fan
- Production controller with live hardware PWM and tach feedback
- Linear interpolation from measured PWM-to-RPM curve data
- Safe start-boost from standstill
- Serial diagnostics for bench bring-up and service checks
- Shared 1-Wire DS18B20 bus on real hardware
- Fixed ROM-ID assignment for one water sensor and one air sensor
- Water-temperature based local control with default target `23.0 C`
- Air-assist control path based on the assigned air sensor
- Target temperature persistence in ESP32 Preferences/NVS
- Clearer alarm and sensor-health diagnostics

Planned next:

- Fault-policy refinement for sensor and fan errors
- Aquarium-side tuning with live temperature data
- MQTT telemetry and remote parameter updates
- OTA updates over Wi-Fi

## Hardware

Bench hardware used so far:

| Component | Quantity | Purpose | Notes |
|---|---:|---|---|
| ESP32 Dev Board | 1 | Main controller | Bench-tested with `esp32:esp32:esp32` target |
| Noctua NF-S12A PWM | 1 | Cooling actuator | 120 mm 4-pin PWM fan |
| DS18B20 | 2 | Water and air temperature | Bench-verified on shared 1-Wire bus |
| 12 V supply | 1 | Fan power | Common ground with ESP32 is mandatory |
| Tach pull-up resistor | 1 | Tach input biasing | Bench-verified at `3.3 kOhm` to `3.3 V` |
| 1-Wire pull-up resistor | 1 | DS18B20 bus biasing | Bench-verified at `3.3 kOhm` to `3.3 V` |

Planned production hardware concept:

| Component | Role |
|---|---|
| ESP32-WROOM-32E / DevKitC-class board | Main controller |
| USB-C PD trigger board | Single-cable 12 V input |
| 12 V to 5 V buck converter | ESP32 supply |
| 3D-printed enclosure | Centralized electronics housing |
| Terminal blocks | Fan and sensor wiring termination |

## Pin Mapping

Current bench mapping:

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| Fan PWM | 25 | 25 kHz PWM output |
| Fan TACH | 26 | Tach input with pull-up |
| 1-Wire bus | 33 | Bench-verified shared DS18B20 bus with `3.3 kOhm` pull-up |

Standard 4-pin PWM fan connector:

| Fan Pin | Signal | Typical Color | Project Connection |
|---|---|---|---|
| 1 | GND | Black | Common GND |
| 2 | +12 V | Yellow | 12 V fan supply |
| 3 | TACH | Green | ESP32 tach input |
| 4 | PWM | Blue | ESP32 PWM output stage |

Important bench note:

- `ESP32 GND`, fan ground, and the 12 V supply ground must share a common reference or the measurements become invalid.
- Verified DS18B20 cable colors for the currently used potted sensors:
  - `rot` -> `3.3 V`
  - `gelb` -> `GND`
  - `gruen` -> `DATA`

## Repository Layout

```text
.
|- README.md
|- docs/
|  |- aquarium-cooling-controller-fsd.md
|  |- design/
|  `- result fan test/
|- firmware/
|  |- controller/
|  `- fan-test/
`- tools/
```

Key files:

- Functional specification: [`docs/aquarium-cooling-controller-fsd.md`](docs/aquarium-cooling-controller-fsd.md)
- Sensor bring-up summary: [`docs/sensor-bringup-2026-04-12.md`](docs/sensor-bringup-2026-04-12.md)
- Fan characterization summary: [`docs/result fan test/measurement-summary-2026-04-12.md`](docs/result%20fan%20test/measurement-summary-2026-04-12.md)
- Controller smoke test: [`docs/result fan test/controller-smoke-test-2026-04-12.md`](docs/result%20fan%20test/controller-smoke-test-2026-04-12.md)
- Characterization sketch: [`firmware/fan-test/fan-test.ino`](firmware/fan-test/fan-test.ino)
- Production controller: [`firmware/controller/controller.ino`](firmware/controller/controller.ino)
- Serial capture helper: [`tools/serial-capture.ps1`](tools/serial-capture.ps1)

## Software Dependencies

Required tools:

- [Arduino IDE](https://www.arduino.cc/en/software) 2.x or compatible `arduino-cli`
- ESP32 board package for Arduino
- Arduino libraries `OneWire` and `DallasTemperature`
- PowerShell on Windows for the serial capture helper

Recommended Arduino board package URL:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Observed during the latest successful bench run:

- FQBN: `esp32:esp32:esp32`
- Running firmware reported active ESP32 Arduino core: `3.3.8`

If your Arduino IDE claims a newer ESP32 package is installed, verify what `arduino-cli` is actually using before debugging firmware behavior.

## Installation

### Option 1: Arduino IDE

1. Install Arduino IDE 2.x.
2. Add the ESP32 board manager URL in `File -> Preferences`.
3. Install the `esp32` board package from `Boards Manager`.
4. Clone this repository:

```bash
git clone https://github.com/TeeVau/aquarium-cooling-controller.git
cd aquarium-cooling-controller
```

5. Open either:
   - `firmware/fan-test/fan-test.ino` for characterization work
   - `firmware/controller/controller.ino` for the current production scaffold

### Option 2: Arduino CLI

Example compile for the controller scaffold:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn esp32:esp32:esp32 firmware/controller
```

Example upload:

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' upload -p COM3 --fqbn esp32:esp32:esp32 firmware/controller
```

## Usage

### 1. Fan Characterization Sketch

Use [`firmware/fan-test/fan-test.ino`](firmware/fan-test/fan-test.ino) to:

- detect start PWM from standstill
- detect minimum hold PWM
- measure the upward and downward PWM-to-RPM curves
- print reusable curve data for the controller

### 2. Production Controller

Use [`firmware/controller/controller.ino`](firmware/controller/controller.ino) for the current local controller.

What it does today:

- initializes PWM output and tach measurement
- initializes a shared DS18B20 bus on `GPIO33`
- assigns water and air sensors by fixed ROM ID
- applies local water-temperature based control
- applies optional air-assist as `max(water_based_pwm, air_based_pwm)`
- persists a user-set target temperature in Preferences/NVS
- falls back to `23.0 C` on invalid or missing target values
- prints the measured fan curve and live diagnostics

Serial commands:

| Command | Effect |
|---|---|
| `status` | Print immediate diagnostics |
| `target <c>` | Set target water temperature in `C` and persist it |
| `default` | Reset target temperature to the default `23.0 C` |
| `airassist` | Print the current air-assist defaults |
| `help` | Show command list |

Expected serial monitor speed:

```text
115200 baud
```

Example session:

```text
target 24.5
status
default
```

## Bench Results

Measured fan behavior on the current bench setup:

- start PWM from standstill: `12%`
- minimum hold PWM while spinning: `10%`
- `0%` PWM: `0 RPM`
- `5%` PWM: `0 RPM`
- `100%` PWM: `1252 RPM`

Current controller verification on hardware:

- compile and flash successful
- fan driver initialization successful
- RPM monitor initialization successful
- two DS18B20 sensors detected on the shared 1-Wire bus
- water sensor ROM fixed at `28333844050000CB`
- air sensor ROM fixed at `28244644050000DA`
- default target temperature verified at `23.0 C`
- invalid target input falls back to `23.0 C`
- persisted target survives reboot
- local water control and first air-assist behavior verified on hardware

Reference artifacts:

- [`docs/sensor-bringup-2026-04-12.md`](docs/sensor-bringup-2026-04-12.md)
- [`docs/result fan test/fan-curve-chart.svg`](docs/result%20fan%20test/fan-curve-chart.svg)
- [`docs/result fan test/controller-startup-2026-04-12.txt`](docs/result%20fan%20test/controller-startup-2026-04-12.txt)
- [`docs/result fan test/controller-smoke-test-2026-04-12.md`](docs/result%20fan%20test/controller-smoke-test-2026-04-12.md)

## Troubleshooting

### Fan does not react to PWM

- Verify common ground between ESP32, fan, and 12 V supply
- Check that PWM is on the real fan PWM pin, not tach or 12 V
- Confirm the board really flashed the intended sketch

### RPM stays at zero

- Check tach pull-up to `3.3 V`
- Verify tach is wired to `GPIO26`
- Confirm the fan model actually provides tach pulses

### Upload fails

- Re-check the selected COM port
- On ESP32 boards, retry while observing reset/boot timing
- Verify the Arduino IDE and `arduino-cli` are using the same ESP32 core installation

### Target temperature behaves unexpectedly

- Run `status` and inspect `Target source` and `Target defaulted`
- Use `default` to clear persisted target temperature and return to `23.0 C`
- If `target <c>` is outside the valid range, the firmware intentionally falls back to `23.0 C`

### Arduino IDE says one ESP32 version, but runtime shows another

- Run `arduino-cli core list`
- Rebuild and reflash after the board package update
- Confirm which installation `arduino-cli` is resolving

## Roadmap

Next likely steps:

1. Refine fan and sensor fault reaction policy
2. Tune water and air control behavior with live aquarium data
3. Add MQTT telemetry and remote parameter updates
4. Add OTA support over Wi-Fi
5. Finalize enclosure and wiring layout for the real aquarium installation

## Contributing

Issues, ideas, measurements, and implementation feedback are welcome.

If you want to contribute code:

1. Open an issue or describe the intended change first.
2. Keep hardware assumptions explicit.
3. Prefer changes that preserve local autonomous operation.
4. Test on real hardware when a change touches PWM, tach, or sensor handling.

## License

This repository is licensed under the GNU General Public License v3.0.

See [LICENSE](LICENSE) for the full license text.

## Project Links

- Repository: [github.com/TeeVau/aquarium-cooling-controller](https://github.com/TeeVau/aquarium-cooling-controller)
- Functional specification: [docs/aquarium-cooling-controller-fsd.md](docs/aquarium-cooling-controller-fsd.md)
